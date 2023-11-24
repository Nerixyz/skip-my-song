# SkipMySong

SkipMySong is a small GUI app to skip the currently playing song with Twitch chat. Its built using `boost::asio` and `wxWidgets`.

![](https://github.com/Nerixyz/skip-my-song/assets/19953266/b5f87563-8ede-438a-b01d-1eab8f9253c7)

| App                      | Supported                                                                            |
| ------------------------ | ------------------------------------------------------------------------------------ |
| Spotify                  | ✅                                                                                   |
| SoundCloud               | ✅                                                                                   |
| YouTube (Chrome/Edge/..) | ✅                                                                                   |
| YouTube (Firefox)        | ❌ <sub><a href="https://bugzilla.mozilla.org/show_bug.cgi?id=1689538">bug</a></sub> |

App or website not listed? SkipMySong works with all apps/websites that support the "skip" media key.

## Building

You need [vcpkg](https://vcpkg.io) installed and in your `PATH` - `VCPKG_ROOT` is the path to your vcpkg installation. If you want to use another package manager, feel free to open a PR/issue.

```powershell
cmake -B build -G Ninja -DVCPKG_TARGET_TRIPLE=x64-windows-static -DCMAKE_TOOLCHAIN_FILE=VCPKG_ROOT\buildsystems\vcpkg.cmake
ninja -C build all
```
