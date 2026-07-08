# Building TapeRot

TapeRot builds on macOS (AU + VST3 + Standalone), Windows (VST3 + Standalone), and Linux
(VST3 + Standalone) — AU is Apple-only. JUCE 8.0.14 is fetched automatically via CMake
`FetchContent` on first configure (no local JUCE checkout needed) on any platform.

## macOS

### Requirements

- Xcode (full install, not just Command Line Tools) — `xcodebuild -version` must succeed.
- CMake 3.24+.
- [pluginval](https://github.com/Tracktion/pluginval) for VST3 validation: `brew install --cask pluginval`.

### Build

```sh
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release
```

This builds AU, VST3, and a Standalone app, and installs the AU/VST3 bundles to:

```
~/Library/Audio/Plug-Ins/Components/TapeRot.component
~/Library/Audio/Plug-Ins/VST3/TapeRot.vst3
```

### Validate

```sh
auval -a | grep -i tapero                    # confirm AU registration + 4-char codes
auval -v aufx Rota Trot                      # full AU validation

/Applications/pluginval.app/Contents/MacOS/pluginval \
    --strictness-level 8 \
    --validate ~/Library/Audio/Plug-Ins/VST3/TapeRot.vst3
```

If Logic Pro doesn't pick up a freshly built AU: Preferences → Audio Units Manager → "Reset & Rescan Selection", or restart Logic.

### Run the unit/DSP tests

```sh
./build/Tests/TapeRotTests_artefacts/Release/TapeRotTests
```

## Windows

### Requirements

- Visual Studio 2022 or later with the "Desktop development with C++" workload.
- CMake 3.24+ (bundled with Visual Studio, or install separately).
- [pluginval](https://github.com/Tracktion/pluginval) for VST3 validation (Windows build available from the same releases page) — no `auval` equivalent, since AU doesn't exist on Windows.

### Build

```bat
cmake -B build -A x64
cmake --build build --config Release
```

This builds VST3 and a Standalone app. VST3 install location is JUCE's own platform default
(`%COMMONPROGRAMFILES%\VST3\TapeRot.vst3`, i.e. usually `C:\Program Files\Common Files\VST3\`) —
TapeRot doesn't override `VST3_COPY_DIR` on Windows.

### Validate

```bat
pluginval.exe --strictness-level 8 --validate "%COMMONPROGRAMFILES%\VST3\TapeRot.vst3"
```

### Run the unit/DSP tests

```bat
build\Tests\TapeRotTests_artefacts\Release\TapeRotTests.exe
```

## Linux

### Requirements

- A C++20-capable compiler (GCC or Clang) and CMake 3.24+.
- JUCE's standard Linux build dependencies:
  ```sh
  sudo apt-get install -y \
      libasound2-dev libjack-jackd2-dev \
      libcurl4-openssl-dev \
      libfreetype6-dev libfontconfig1-dev \
      libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
      libwebkit2gtk-4.1-dev \
      libglu1-mesa-dev mesa-common-dev
  ```
  (package names above are for Debian/Ubuntu — adjust for other distros).
- [pluginval](https://github.com/Tracktion/pluginval) for VST3 validation (Linux build available from the same releases page) — no `auval` equivalent, since AU doesn't exist on Linux.

### Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Unlike the Xcode/Visual Studio generators used on macOS/Windows, CMake's default Linux generators
(Makefiles/Ninja) are single-config, so `CMAKE_BUILD_TYPE=Release` must be set at configure time.

This builds VST3 and a Standalone app. VST3 install location is JUCE's own platform default
(`~/.vst3/TapeRot.vst3`) — TapeRot doesn't override `VST3_COPY_DIR` on Linux.

### Validate

```sh
./pluginval --strictness-level 8 --validate ~/.vst3/TapeRot.vst3
```

### Run the unit/DSP tests

```sh
./build/Tests/TapeRotTests_artefacts/Release/TapeRotTests
```

## What the DSP test suite covers

Exact-null at `drive = 0`, bounded/finite output at full drive, fixed-delay behavior at `wow = flutter = 0`, bounded output under full wow/flutter modulation, no large discontinuity across a MODEL switch, and a CPU check (average `processBlock` time at 48kHz/64-sample buffers vs. the ~1.33ms real-time budget).

## Notes

- `PLUGIN_MANUFACTURER_CODE` (`Trot`), `PLUGIN_CODE` (`Rota`), `BUNDLE_ID` (`com.taperot.taperot`), and `COMPANY_NAME` in `CMakeLists.txt` are placeholders — finalize these before any real release, since they're effectively permanent once shipped or automated against.
- JUCE's free/personal tier splash screen is enabled (no paid license configured).
- All signal-chain stages have real DSP implemented (drive/saturation, wow & flutter, the 8-model GEN-cascading tape EQ with FADE/CLUNK switching, noise/hum, the failure engine, LP/HP tone shaping, STOP/FILTER/FAIL performance triggers, stereo spread, mix/output). The one intentional placeholder is the REVOX B77 model's EQ curve, deliberately near-transparent pending measured reference data.
