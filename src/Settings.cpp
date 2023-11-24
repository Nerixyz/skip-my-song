#include "Settings.hpp"

#include <winrt/Windows.Data.Json.h>

#include <wx/log.h>

#include <filesystem>
#include <fstream>
#include <print>

#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <Windows.h>
#include <tchar.h>
#include <ShlObj.h>
#include <KnownFolders.h>
// clang-format on

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Data::Json;

namespace
{

std::filesystem::path getSettingsPath()
{
  PWSTR appdata;
  if (!SUCCEEDED(
          SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appdata)))
  {
    return std::filesystem::current_path();
  }
  std::filesystem::path path(std::wstring_view(appdata, std::wcslen(appdata)));
  CoTaskMemFree(appdata);
  return path / "SkipMySong";
}

void ensurePath(const std::filesystem::path &path)
{
  if (std::filesystem::exists(path))
  {
    return;
  }

  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec)
  {
    wxLogMessage("Failed to create directories: %s", ec.message());
  }
}

Rules defaultRules()
{
  return Rules{
      .command = "-voteskip",
      .channel = "nerixyz",
      .allowSubs = true,
      .allowNonSubs = true,
      .threshold = 42,
  };
}

} // namespace

AppSettings::AppSettings()
    : configDir_(getSettingsPath()),
      configPath_(this->configDir_ / "skip-my-song.json")
{
  ensurePath(this->configDir_);
  std::println(stderr, "Settings path is {}", this->configPath_.string());
}

Rules AppSettings::read() const
{
  try
  {
    if (!std::filesystem::exists(this->configPath_))
    {
      std::println(stderr, "Using default settings as no config was found");
      return defaultRules();
    }
    std::wifstream in(this->configPath_);
    std::wstringstream stream;
    stream << in.rdbuf();

    winrt::hstring source(stream.view());

    JsonObject obj;
    if (!JsonObject::TryParse(source, obj))
    {
      std::println(stderr, "Settings contain invalid JSON");
      return defaultRules();
    }
    std::println(stderr, "Loaded settings");
    return Rules{
        .command =
            winrt::to_string(obj.GetNamedString(L"command", L"-voteskip")),
        .channel = winrt::to_string(obj.GetNamedString(L"channel", L"nerixyz")),
        .allowSubs = obj.GetNamedBoolean(L"allowSubs", true),
        .allowNonSubs = obj.GetNamedBoolean(L"allowNonSubs", true),
        .threshold = static_cast<size_t>(
            std::max(1.0, obj.GetNamedNumber(L"threshold", 42.0)))};
  }
  catch (const std::exception &ex)
  {
    wxLogMessage("Failed to read settings: %s", ex.what());
    std::println(stderr, "Failed to read settings: {}", ex.what());
  }
  return defaultRules();
}

void AppSettings::writeNow(const Rules &rules) const
{
  try
  {
    JsonObject obj;
    obj.SetNamedValue(L"command", JsonValue::CreateStringValue(
                                      winrt::to_hstring(rules.command)));
    obj.SetNamedValue(L"channel", JsonValue::CreateStringValue(
                                      winrt::to_hstring(rules.channel)));
    obj.SetNamedValue(L"allowSubs",
                      JsonValue::CreateBooleanValue(rules.allowSubs));
    obj.SetNamedValue(L"allowNonSubs",
                      JsonValue::CreateBooleanValue(rules.allowNonSubs));
    obj.SetNamedValue(L"threshold", JsonValue::CreateNumberValue(
                                        static_cast<double>(rules.threshold)));
    std::wofstream out(this->configPath_);
    out << obj.Stringify();
    std::println(stderr, "Saved settings");
  }
  catch (const std::exception &ex)
  {
    wxLogMessage("Failed to write settings: %s", ex.what());
    std::println(stderr, "Failed to write settings: {}", ex.what());
  }
}

winrt::fire_and_forget AppSettings::writeBackground(Rules rules)
{
  auto lifetime = this->get_strong();
  co_await winrt::resume_background();
  this->writeNow(rules);
}
