I'm  using AI to modify a dead tool so bare with me

# ScreamAPI – Achievement Manager Edition

This is a custom build of **ScreamAPI** (based on v1.13.0-1) that reintroduces the achievement manager overlay and adds compatibility with newer Epic Online Services (EOS) SDK features. The original achievement manager was removed after v2.0.0; this fork brings it back and ensures it works with games that require EOS SDK 1.15+.

---

## ✨ Features

- **DLC unlocking** – same core functionality as original ScreamAPI.
- **Achievement manager overlay** – an in‑game overlay that displays all Epic Achievements for the current game, shows unlock progress, and allows manual unlocking (if the game supports it).
- **Updated EOS SDK compatibility** – we kept the original v1.13.0-1 codebase (which includes the overlay) but updated the EOS SDK headers to **1.17.1.3** and added mandatory platform status calls required for EOS SDK v1.15+.
- **Platform status compatibility** – added calls to `EOS_Platform_SetApplicationStatus` and `EOS_Platform_SetNetworkStatus` (required since EOS SDK v1.15).
- **Fallback platform handle capture** – attempts to capture the platform handle from the game via getter functions (`EOS_Platform_GetAchievementsInterface`, etc.) when `EOS_Platform_Create` is not called.
- **Comprehensive logging** – detailed debug output to help diagnose integration issues.

---

## 🔧 Why This Version Instead of ScreamAPI 4.0.0?

**ScreamAPI 4.0.0 removed the achievement manager overlay entirely.**  
While v4.0.0 has a more modern codebase and CMake build system, it lacks the overlay functionality. This fork is based on the last version that still included the overlay (v1.13.0-1) and backports necessary SDK updates to keep it compatible with modern games that require EOS SDK 1.15+.

The overlay code was originally written for an older EOS SDK (pre‑v1.15). To make it work with newer games, we:
- Replaced the old EOS SDK headers with those from **1.17.1.3**.
- Added the mandatory platform status calls (`SetApplicationStatus`, `SetNetworkStatus`) that are required in SDK v1.15+.
- Updated achievement‑related structs and callbacks to match the newer API.

---

## Download
You can always download the latest ScreamAPI release in this repository at the following address:
https://github.com/acidicoala/ScreamAPI/releases/latest

## Acknowledgements

This project makes use of several open-source libraries listed below:

* [inih](https://github.com/benhoyt/inih)
* [stb](https://github.com/nothings/stb)
* [curl](https://github.com/curl/curl)
* [kiero](https://github.com/Rebzzel/kiero)
* [imgui](https://github.com/ocornut/imgui)
* [minhook](https://github.com/TsudaKageyu/minhook)

## License
This software is licensed under [Zero Clause BSD](https://en.wikipedia.org/wiki/BSD_licenses#0-clause_license_(%22BSD_Zero_Clause_License%22,[14]_%22Zero-Clause_BSD%22,[15]_or_%22Free_Public_License_1.0.0%22[15])) license, terms of which are available in [LICENSE.txt](LICENSE.txt)
