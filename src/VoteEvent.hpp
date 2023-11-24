#pragma once

#include <wx/event.h>

class VoteEvent;
wxDECLARE_EVENT(VOTE_EVENT, VoteEvent);

class VoteEvent : public wxEvent
{
public:
  VoteEvent(std::string user) : wxEvent(0, VOTE_EVENT), user_(std::move(user))
  {
  }

  wxEvent *Clone() const override { return new VoteEvent(this->user_); }

  const std::string &user() const { return this->user_; }

private:
  std::string user_;
};
