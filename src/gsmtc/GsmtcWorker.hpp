#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

class GsmtcWorker
    : public winrt::implements<GsmtcWorker,
                               winrt::Windows::Foundation::IInspectable>
{
public:
  GsmtcWorker();
  winrt::Windows::Foundation::IAsyncAction init();

  winrt::fire_and_forget skipSong();

private:
  winrt::Windows::Media::Control::
      GlobalSystemMediaTransportControlsSessionManager manager_;
};
