# Building TapeRot

## Requirements

- Xcode (full install, not just Command Line Tools) — `xcodebuild -version` must succeed.
- CMake 3.24+.
- [pluginval](https://github.com/Tracktion/pluginval) for VST3 validation: `brew install --cask pluginval`.

JUCE 8.0.14 is fetched automatically via CMake `FetchContent` on first configure (no local JUCE checkout needed).

## Build

```sh
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release
```

This builds AU, VST3, and a Standalone app, and installs the AU/VST3 bundles to:

```
~/Library/Audio/Plug-Ins/Components/TapeRot.component
~/Library/Audio/Plug-Ins/VST3/TapeRot.vst3
```

## Validate

```sh
auval -a | grep -i tapero                    # confirm AU registration + 4-char codes
auval -v aufx Rota Trot                      # full AU validation

/Applications/pluginval.app/Contents/MacOS/pluginval \
    --strictness-level 8 \
    --validate ~/Library/Audio/Plug-Ins/VST3/TapeRot.vst3
```

If Logic Pro doesn't pick up a freshly built AU: Preferences → Audio Units Manager → "Reset & Rescan Selection", or restart Logic.

## Run the unit/DSP tests

```sh
./build/Tests/TapeRotTests_artefacts/Release/TapeRotTests
```

Covers: exact-null at `drive = 0`, bounded/finite output at full drive, fixed-delay behavior at `wow = flutter = 0`, bounded output under full wow/flutter modulation, no large discontinuity across a MODEL switch, and a CPU check (average `processBlock` time at 48kHz/64-sample buffers vs. the ~1.33ms real-time budget).

## Notes

- `PLUGIN_MANUFACTURER_CODE` (`Trot`), `PLUGIN_CODE` (`Rota`), `BUNDLE_ID` (`com.taperot.taperot`), and `COMPANY_NAME` in `CMakeLists.txt` are placeholders — finalize these before any real release, since they're effectively permanent once shipped or automated against.
- JUCE's free/personal tier splash screen is enabled (no paid license configured).
- All signal-chain stages have real DSP implemented (drive/saturation, wow & flutter, the 8-model GEN-cascading tape EQ with FADE/CLUNK switching, noise/hum, the failure engine, LP/HP tone shaping, STOP/FILTER/FAIL performance triggers, stereo spread, mix/output). The one intentional placeholder is the REVOX B77 model's EQ curve, deliberately near-transparent pending measured reference data.
