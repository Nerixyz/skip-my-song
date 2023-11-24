#include "gsmtc/GsmtcWorker.hpp"

#include <wx/log.h>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Control;
using namespace std::string_literals;

GsmtcWorker::GsmtcWorker() : manager_(nullptr) {}

IAsyncAction GsmtcWorker::init()
{
  auto lifetime = this->get_strong();
  try
  {
    this->manager_ = co_await GlobalSystemMediaTransportControlsSessionManager::
        RequestAsync();
  }
  catch (const std::exception &ex)
  {
    wxLogMessage("Failed to get GSMTC manager: %s", ex.what());
  }
}

winrt::fire_and_forget GsmtcWorker::skipSong()
{
  auto lifetime = this->get_strong();
  try
  {
    if (!this->manager_)
    {
      wxLogMessage("No GSMTC manager found");
      co_return;
    }

    co_await winrt::resume_background();

    auto session = this->manager_.GetCurrentSession();
    if (!session)
    {
      wxLogMessage("No app is playing any song");
      co_return;
    }
    auto playbackInfo = session.GetPlaybackInfo();
    if (playbackInfo)
    {
      if (playbackInfo.PlaybackStatus() !=
          GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
      {
        wxLogMessage("Current app isn't playing any song");
        co_return;
      }
    }

    std::string title = "[?]"s;
    auto mediaProperties = co_await session.TryGetMediaPropertiesAsync();
    if (mediaProperties)
    {
      title = winrt::to_string(mediaProperties.Title());
    }

    // This is specifically for YouTube which does not register
    // a handler for 'nexttrack' and for some reason, GSMTC thinks
    // we skip the current song.
    //
    // Here, we try to seek to the end of the song first and then skip it.
    auto tl = session.GetTimelineProperties();
    if (tl)
    {
      auto target = std::max(tl.MaxSeekTime(), tl.EndTime());
      co_await session.TryChangePlaybackPositionAsync(target.count());
    }

    auto skipped = co_await session.TrySkipNextAsync();
    if (skipped)
    {
      wxLogMessage("Skipped %s", title);
    }
    else
    {
      wxLogMessage("Failed to skip %s", title);
    }
  }
  catch (const std::exception &ex)
  {
    wxLogMessage("Failed to skip song: %s", ex.what());
  }
}
