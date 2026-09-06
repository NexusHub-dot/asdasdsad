# Build Pulse Macro v1.4.9 for Windows

Target: Geometry Dash 2.2081, Geode 5.10.1, Win64.

## Easiest: GitHub Actions

This source includes `.github/workflows/build-win64.yml`.

1. Put this source tree in a GitHub repository.
2. Open **Actions → Build Win64 Geode → Run workflow**.
3. Download the `PulseMacro-v1.4.9-Win64` artifact.
4. Put the generated `.geode` file in Geometry Dash's `geode/mods` directory.

The included workflow uses short Windows paths (`C:\p` for the project, `C:\g` for Geode SDK, `C:\b` for the build) and Geode SDK v5.10.1. This avoids Windows MAX_PATH failures when the GitHub repository name is unusually long. It still runs the standalone tests first and uploads the compiled `.geode` as an Actions artifact.

## Local Windows build

Install Geode CLI + SDK 5.10.1 and a supported C++ toolchain, then from this folder configure/build in Release. The project expects `GEODE_SDK` to point at your SDK checkout.

The SDK path is cached as `PULSE_GEODE_SDK` so regeneration does not accidentally switch SDK versions if the environment later changes. To choose it explicitly:

```powershell
cmake -S . -B build -DPULSE_GEODE_SDK="C:/path/to/geode-sdk-5.10.1" -DPULSE_INTEGRATION_TEST=OFF
cmake --build build --config Release --parallel 6
ctest --test-dir build -C Release --output-on-failure
```

The checkout needs the matching prebuilt Geode link library. No automatic installation into GD is performed (`DONT_INSTALL`). This revision was compiled using Geode SDK commit `7e41336f68660b990644f5c3450b6da6ae944560` and bindings commit `7f6c2a75742856de88dad354e576dcff8a28e881`. To reproduce the same bindings, check out that revision and pass `-DGEODE_BINDINGS_REPO_PATH="C:/path/to/bindings"`.

Standalone logic tests can be run without the Geode SDK:

```sh
cmake -S . -B build-core -DPULSE_CORE_ONLY=ON -DBUILD_TESTING=ON
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```
