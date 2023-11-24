#pragma once

#include "Rules.hpp"
#include "VoteEvent.hpp"

#include <wx/event.h>

#include <boost/asio/experimental/concurrent_channel.hpp>

#include <memory>
#include <mutex>

struct AppContext
{
  struct Ping
  {
  };
  using PingChannel = boost::asio::experimental::concurrent_channel<void(
      boost::system::error_code, Ping)>;

  template <typename T>
  AppContext(const T &executionContext, wxEvtHandler *voteHandler)
      : rulesChanged_(executionContext, 1),
        voteHandler_(voteHandler)
  {
  }

  AppContext(const AppContext &) = delete;
  AppContext(AppContext &&) noexcept = delete;
  AppContext &operator=(const AppContext &) = delete;
  AppContext &operator=(AppContext &&) noexcept = delete;

  PingChannel &rulesChanged() { return this->rulesChanged_; }

  Rules readRules()
  {
    this->rulesMtx_.lock();
    auto rules = this->rules_;
    this->rulesMtx_.unlock();
    return rules;
  }
  void setRules(Rules rules, bool emit = true)
  {
    this->rulesMtx_.lock();
    this->rules_ = std::move(rules);
    this->rulesMtx_.unlock();
    if (emit)
    {
      this->rulesChanged_.try_send(boost::system::error_code{}, Ping{});
    }
  }

  void publishVote(std::string user)
  {
    auto *handler = this->voteHandler_.load();
    if (handler == nullptr)
    {
      return;
    }
    handler->QueueEvent(new VoteEvent(std::move(user)));
  }

  void setHandler(wxEvtHandler *voteHandler)
  {
    assert(this->voteHandler_.load() == nullptr);
    this->voteHandler_.store(voteHandler);
  }

private:
  PingChannel rulesChanged_;
  Rules rules_;
  std::mutex rulesMtx_;

  std::atomic<wxEvtHandler *> voteHandler_;
};

using AppContextPtr = std::shared_ptr<AppContext>;
