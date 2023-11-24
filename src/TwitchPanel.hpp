#pragma once

#include "AppContext.hpp"
#include "Settings.hpp"
#include "gsmtc/GsmtcWorker.hpp"

#include <wx/panel.h>
#include <wx/timer.h>

#include <unordered_set>

class wxStaticText;
class wxTextCtrl;
class wxCheckBox;
class wxSpinCtrl;

class TwitchPanel : public wxPanel
{
public:
  TwitchPanel(wxWindow *parent, AppContextPtr app,
              winrt::com_ptr<GsmtcWorker> gsmtc,
              winrt::com_ptr<AppSettings> settings);

private:
  enum Id
  {
    ConnectBtn = 0x1000,
    ChannelBox,
    CommandBox,
    ThresholdBox,
    AllowSubsChk,
    AllowNonSubsChk,
    ToggleStateBtn,
    ResetVotesBtn,

    CommandDebounceTimer = 0x2000,
    ThresholdDebounceTimer,
    SettingsDebounceTimer,
  };

  void connect(wxCommandEvent &evt);
  void permissionsUpdated(wxCommandEvent &evt);
  void thresholdUpdated(wxSpinEvent &evt);
  void applyThreshold(wxTimerEvent &evt);

  void resetVotes();
  void resetVotes(wxCommandEvent &evt);

  void onVote(const std::string &user);

  void emitRules();
  void queueSave();
  void doSave(wxTimerEvent &evt);

  void toggleState(wxCommandEvent &evt);

  wxStaticText *currentVotesLabel_ = nullptr;
  wxTextCtrl *channelCtrl_ = nullptr;
  wxTextCtrl *commandCtrl_ = nullptr;
  wxCheckBox *allowNonSubsBox_ = nullptr;
  wxCheckBox *allowSubsBox_ = nullptr;
  wxSpinCtrl *minVotesCtrl_ = nullptr;
  wxButton *toggleBtn_ = nullptr;

  wxTimer commandDebouncer_;
  wxTimer thresholdDebouncer_;
  wxTimer settingsDebouncer_;

  size_t currentVotes_ = 0;
  bool enabled_ = true;

  AppContextPtr app_;
  Rules rules_;

  winrt::com_ptr<GsmtcWorker> gsmtc_;
  winrt::com_ptr<AppSettings> settings_;

  std::unordered_set<std::string> votes_;

  wxDECLARE_EVENT_TABLE();
};
