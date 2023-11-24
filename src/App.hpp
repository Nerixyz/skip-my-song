#pragma once

#include "AppContext.hpp"
#include "Settings.hpp"

#include <wx/app.h>

class App : public wxApp
{
public:
  bool OnInit() override;
  int OnExit() override;

private:
  void initContext(Rules initialRules);

  AppContextPtr app_;
  winrt::com_ptr<AppSettings> settings_;
};

wxDECLARE_APP(App);
