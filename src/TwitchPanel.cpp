#include "TwitchPanel.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace
{

constexpr auto BORDER_X = wxRIGHT | wxLEFT;

} // namespace

TwitchPanel::TwitchPanel(wxWindow *parent, AppContextPtr app,
                         winrt::com_ptr<GsmtcWorker> gsmtc,
                         winrt::com_ptr<AppSettings> settings)
    : wxPanel(parent),
      commandDebouncer_(this, Id::CommandDebounceTimer),
      thresholdDebouncer_(this, Id::ThresholdDebounceTimer),
      settingsDebouncer_(this, Id::SettingsDebounceTimer),
      app_(std::move(app)),
      rules_(this->app_->readRules()),
      gsmtc_(std::move(gsmtc)),
      settings_(std::move(settings))
{
  auto *sizerPanel = new wxBoxSizer(wxVERTICAL);

  // Channel
  auto *channelBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Channel");
  this->channelCtrl_ =
      new wxTextCtrl(this, Id::ChannelBox, this->rules_.channel,
                     wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
  channelBox->Add(this->channelCtrl_, 1);
  channelBox->AddSpacer(5);
  channelBox->Add(new wxButton(this, Id::ConnectBtn, "Connect"));
  sizerPanel->Add(channelBox, 0, wxEXPAND | wxRIGHT, 3);
  sizerPanel->AddSpacer(10);

  // Command
  auto *connectBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Command");
  this->commandCtrl_ =
      new wxTextCtrl(this, Id::CommandBox, this->rules_.command);
  connectBox->Add(this->commandCtrl_, 1, wxEXPAND);
  sizerPanel->Add(connectBox, 0, wxEXPAND | wxRIGHT, 3);
  sizerPanel->AddSpacer(10);

  // Misc. Settings
  auto *settingsSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Votes");
  auto *votePerms = new wxBoxSizer(wxHORIZONTAL);
  auto *voteBox = new wxBoxSizer(wxVERTICAL);
  // Threshold & Votes
  auto *thresholdBox = new wxBoxSizer(wxHORIZONTAL);
  thresholdBox->Add(new wxStaticText(this, wxID_ANY, "Threshold"), 0,
                    wxALIGN_CENTER | wxRIGHT, 5);
  this->minVotesCtrl_ =
      new wxSpinCtrl(this, Id::ThresholdBox, {}, wxDefaultPosition,
                     wxDefaultSize, wxSP_ARROW_KEYS, 1, 1024, 42);
  this->minVotesCtrl_->SetValue(static_cast<int>(this->rules_.threshold));
  thresholdBox->Add(this->minVotesCtrl_, 0, wxALIGN_CENTER);
  voteBox->Add(thresholdBox, 0, wxALIGN_CENTER | wxBOTTOM, 8);
  this->currentVotesLabel_ =
      new wxStaticText(this, wxID_ANY, "Current Votes: 0");
  voteBox->Add(this->currentVotesLabel_, 0, wxALIGN_CENTER);
  votePerms->Add(voteBox, 1, wxALIGN_CENTER | BORDER_X, 5);

  // Permissions (Subs/Non-Subs)
  auto *permissionBox = new wxBoxSizer(wxVERTICAL);
  this->allowSubsBox_ = new wxCheckBox(this, Id::AllowSubsChk, "Allow Subs");
  this->allowSubsBox_->SetValue(this->rules_.allowSubs);
  permissionBox->Add(this->allowSubsBox_);

  this->allowNonSubsBox_ =
      new wxCheckBox(this, Id::AllowNonSubsChk, "Allow Non-Subs");
  this->allowNonSubsBox_->SetValue(this->rules_.allowNonSubs);
  permissionBox->Add(this->allowNonSubsBox_);
  votePerms->Add(permissionBox, 1, wxALIGN_CENTER | BORDER_X, 5);

  settingsSizer->Add(votePerms, 0, wxEXPAND | wxBOTTOM, 5);

  auto *contolVotes = new wxBoxSizer(wxHORIZONTAL);
  this->toggleBtn_ = new wxButton(this, Id::ToggleStateBtn, "Disable");
  contolVotes->Add(this->toggleBtn_, 1);
  contolVotes->Add(new wxButton(this, Id::ResetVotesBtn, "Reset Votes"), 1);
  settingsSizer->Add(contolVotes, 0, wxEXPAND);

  sizerPanel->Add(settingsSizer, 0, wxEXPAND | wxRIGHT, 3);
  sizerPanel->AddSpacer(10);

  // Logs
  auto *logCtrl =
      new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                     wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  // TODO: delete?
  wxLog::SetActiveTarget(new wxLogTextCtrl(logCtrl));
  auto *logBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Logs");
  logBox->Add(logCtrl, 1, wxEXPAND);
  sizerPanel->Add(logBox, 1, wxEXPAND | wxRIGHT, 3);

  this->SetSizer(sizerPanel);

  this->Bind(
      wxEVT_TIMER, [this](auto) { this->emitRules(); },
      Id::CommandDebounceTimer);
  this->Bind(
      wxEVT_TEXT, [this](auto) { this->commandDebouncer_.StartOnce(1000); },
      Id::CommandBox);

  this->Bind(VOTE_EVENT,
             [this](const VoteEvent &event) { this->onVote(event.user()); });

  this->app_->setHandler(this);
  this->emitRules();
}

void TwitchPanel::onVote(const std::string &user)
{
  if (!this->enabled_)
  {
    return;
  }

  auto [_, inserted] = this->votes_.emplace(user);
  if (!inserted)
  {
    return;
  }
  this->currentVotes_ += 1;
  this->currentVotesLabel_->SetLabel(
      std::format("Current Votes: {}", this->currentVotes_));

  if (this->currentVotes_ >= this->rules_.threshold)
  {
    wxLogMessage("Votes reached!");
    this->gsmtc_->skipSong();
    this->resetVotes();
  }
}

void TwitchPanel::emitRules()
{
  this->rules_ = Rules{
      .command = this->commandCtrl_->GetValue().ToStdString(),
      .channel = this->channelCtrl_->GetValue().ToStdString(),
      .allowSubs = this->allowSubsBox_->GetValue(),
      .allowNonSubs = this->allowNonSubsBox_->GetValue(),
      .threshold = this->rules_.threshold,
  };
  this->app_->setRules(this->rules_);
  this->queueSave();
}

void TwitchPanel::connect(wxCommandEvent & /*evt*/) { this->emitRules(); }
void TwitchPanel::permissionsUpdated(wxCommandEvent & /*evt*/)
{
  this->emitRules();
}
void TwitchPanel::thresholdUpdated(wxSpinEvent & /*evt*/)
{
  this->thresholdDebouncer_.StartOnce(1000);
}

void TwitchPanel::applyThreshold(wxTimerEvent & /*evt*/)
{
  this->rules_.threshold =
      static_cast<size_t>(std::max(this->minVotesCtrl_->GetValue(), 1));
  if (this->currentVotes_ >= this->rules_.threshold)
  {
    this->resetVotes();
  }
  this->queueSave();
}

void TwitchPanel::resetVotes()
{
  this->currentVotes_ = 0;
  this->votes_.clear(); // TODO: resize?
  this->currentVotesLabel_->SetLabel(
      std::format("Current Votes: {}", this->currentVotes_));
  wxLogMessage("Reset votes");
}

void TwitchPanel::resetVotes(wxCommandEvent & /*evt*/) { this->resetVotes(); }

void TwitchPanel::doSave(wxTimerEvent & /*evt*/)
{
  this->settings_->writeBackground(this->rules_);
}

void TwitchPanel::queueSave()
{
  if (!this->settingsDebouncer_.IsRunning())
  {
    this->settingsDebouncer_.StartOnce(1000);
  }
}

void TwitchPanel::toggleState(wxCommandEvent & /*evt*/)
{
  if (this->enabled_)
  {
    this->enabled_ = false;
    this->toggleBtn_->SetLabel("Enable");
    wxLogMessage("Disabled voting");
  }
  else
  {
    this->enabled_ = true;
    this->toggleBtn_->SetLabel("Disable");
    wxLogMessage("Enabled voting");
  }
}

// clang-format off
wxBEGIN_EVENT_TABLE(TwitchPanel, wxPanel)
    EVT_BUTTON(Id::ConnectBtn, TwitchPanel::connect)
    EVT_TEXT_ENTER(Id::ChannelBox, TwitchPanel::connect)
    EVT_CHECKBOX(Id::AllowSubsChk, TwitchPanel::permissionsUpdated)
    EVT_CHECKBOX(Id::AllowNonSubsChk, TwitchPanel::permissionsUpdated)
    EVT_SPINCTRL(Id::ThresholdBox, TwitchPanel::thresholdUpdated)
    EVT_TIMER(Id::ThresholdDebounceTimer, TwitchPanel::applyThreshold)
    EVT_TIMER(Id::SettingsDebounceTimer, TwitchPanel::doSave)
    EVT_BUTTON(Id::ToggleStateBtn, TwitchPanel::toggleState)
    EVT_BUTTON(Id::ResetVotesBtn, TwitchPanel::resetVotes)
wxEND_EVENT_TABLE()
    // clang-format on
