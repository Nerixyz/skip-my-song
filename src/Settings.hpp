#pragma once

#include "AppContext.hpp"

#include <winrt/Windows.Foundation.h>

#include <filesystem>

class AppSettings
    : public winrt::implements<AppSettings,
                               winrt::Windows::Foundation::IInspectable>
{
public:
  AppSettings();

  Rules read() const;

  void writeNow(const Rules &rules) const;
  winrt::fire_and_forget writeBackground(Rules rules);

private:
  std::filesystem::path configDir_;
  std::filesystem::path configPath_;
};
