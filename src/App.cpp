#include "App.hpp"

#include "AppContext.hpp"
#include "Settings.hpp"
#include "TwitchPanel.hpp"
#include "gsmtc/GsmtcWorker.hpp"
#include "irc/IrcClient.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <condition_variable>
#include <mutex>
#include <print>
#include <thread>
#include <utility>

wxIMPLEMENT_APP(App);

class RootFrame : public wxFrame
{
public:
  RootFrame(AppContextPtr app, winrt::com_ptr<GsmtcWorker> gsmtc,
            winrt::com_ptr<AppSettings> settings);
};

void App::initContext(Rules initialRules)
{
  std::condition_variable condvar;
  std::mutex mtx;
  std::thread(
      [&]
      {
        IrcClient client;
        client.run(
            [&](auto init)
            {
              {
                std::lock_guard<std::mutex> lk(mtx);
                this->app_ = std::move(init);
                this->app_->setRules(initialRules, false);
              }
              condvar.notify_all();
            });
      })
      .detach(); // TODO: don't

  std::unique_lock<std::mutex> lk(mtx);
  condvar.wait(lk, [&] { return this->app_ != nullptr; });
}

bool App::OnInit()
{
  if (!wxApp::OnInit())
  {
    return false;
  }
  winrt::uninit_apartment();
  winrt::init_apartment();

  wxLog::EnableLogging();

  auto gsmtc = winrt::make_self<GsmtcWorker>();
  gsmtc->init().get();
  this->settings_ = winrt::make_self<AppSettings>();

  this->initContext(this->settings_->read());

  // Create the main window
  auto *frame = new RootFrame(this->app_, std::move(gsmtc), this->settings_);

  frame->Show();

  return true;
}

int App::OnExit()
{
  this->settings_->writeNow(this->app_->readRules());
  return wxApp::OnExit();
}

RootFrame::RootFrame(AppContextPtr app, winrt::com_ptr<GsmtcWorker> gsmtc,
                     winrt::com_ptr<AppSettings> settings)
    : wxFrame(nullptr, wxID_ANY, "SkipMySong")
{
  auto *panel = new wxPanel(this);
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  auto *book =
      new wxNotebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
  book->Hide();

  auto *page = new TwitchPanel(book, std::move(app), std::move(gsmtc),
                               std::move(settings));
  book->AddPage(page, "Twitch", false);

  book->SetSelection(0);

  sizer->Insert(0, book, wxSizerFlags(5).Expand().Border());
  sizer->Show(book);
  sizer->Layout();
  panel->SetSizer(sizer);
  panel->Layout();

  auto *wrapSize = new wxBoxSizer(wxVERTICAL);
  wrapSize->Add(panel, wxSizerFlags(1).Expand());
  this->SetSizerAndFit(wrapSize);
  this->SetMinSize({300, 400});
}
