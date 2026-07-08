# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

TapeRot builds on macOS (AU + VST3 + Standalone), Windows (VST3 + Standalone), and Linux
(VST3 + Standalone) — AU is Apple-only; `CMakeLists.txt` conditionalizes `FORMATS`/`*_COPY_DIR` on
`APPLE`, so Windows and Linux share the same non-Apple branch (`VST3 Standalone`, unset copy dirs →
JUCE's own per-OS default). JUCE 8.0.14 is fetched automatically via CMake `FetchContent` on any
platform, no local checkout needed.

Configure once — macOS:

```sh
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
```

Configure once — Windows:

```bat
cmake -B build -A x64
```

Configure once — Linux (single-config generator, so `CMAKE_BUILD_TYPE` must be set here rather than only at build time):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Re-run the configure step whenever `CMakeLists.txt` changes (new sources, new `juce_add_plugin` args, etc.) — a plain rebuild won't pick those up.

Build (macOS auto-installs AU/VST3 to `~/Library/Audio/Plug-Ins/...`; Windows installs VST3 to JUCE's own default, `%COMMONPROGRAMFILES%\VST3\`; Linux installs VST3 to JUCE's own default, `~/.vst3/`):

```sh
cmake --build build --config Release
```

Run the DSP unit tests (Catch2-style, built as a console app target `TapeRotTests`):

```sh
./build/Tests/TapeRotTests_artefacts/Release/TapeRotTests          # macOS / Linux
build\Tests\TapeRotTests_artefacts\Release\TapeRotTests.exe        # Windows
```

To run a single test, pass its name/tag as an argument to that binary (Catch2 CLI conventions — e.g. `./build/Tests/TapeRotTests_artefacts/Release/TapeRotTests "[saturator]"`).

Validate plugin formats after building — macOS (AU + VST3):

```sh
auval -a | grep -i tapero                    # confirm AU registration + 4-char codes
auval -v aufx Rota Trot                      # full AU validation

/Applications/pluginval.app/Contents/MacOS/pluginval \
    --strictness-level 8 \
    --validate ~/Library/Audio/Plug-Ins/VST3/TapeRot.vst3
```

Validate on Windows (VST3 only, no AU/`auval` equivalent):

```bat
pluginval.exe --strictness-level 8 --validate "%COMMONPROGRAMFILES%\VST3\TapeRot.vst3"
```

Validate on Linux (VST3 only, no AU/`auval` equivalent):

```sh
./pluginval --strictness-level 8 --validate ~/.vst3/TapeRot.vst3
```

If Logic Pro doesn't pick up a freshly built AU: Audio Units Manager → "Reset & Rescan Selection", or restart Logic. If a rebuilt app/plugin icon doesn't refresh, LaunchServices/Dock/Finder are caching by bundle path — `touch` the bundle, re-run `lsregister -f <bundle>`, then `killall Dock Finder`. (macOS-only tips — on Windows/Linux, a host's own plugin rescan action, e.g. Reaper's "re-scan VST paths", covers the equivalent case.)

See [BUILDING.md](BUILDING.md) for full per-platform requirements (Xcode/Visual Studio, CMake 3.24+, pluginval).

## Prompts log

`prompts/PROMPTS.md` holds numbered work-package prompts. Once a prompt has been fully implemented, mark it `SHIPPED` with the date it shipped (e.g. `PROMPT #1 - SHIPPED 2026-07-05`) instead of `TODO`.

## Architecture

### Signal chain (fixed order, all in `PluginProcessor::processBlock`)

```
Saturator -> [GEN-cascade of 1-8 DegradationCore stages] -> Hum -> FailureEngine -> StereoSpread
    -> ToneFilters (LP/HP) -> TapeStop (STOP aux) -> FilterSweep (FILTER aux) -> OutputStage
```

Each `DegradationCore` stage is itself `WowFlutter -> TapeModelEQ -> NoiseSource`, reusing the same
model-EQ table (`TapeModelData.h`) per generation. `GEN` selects how many stages run (1-8); the
processor cross-fades between the floor/ceil stage count as GEN is automated (`genSmoothed`,
`genFloorSnapshot`) so raising/lowering it mid-playback doesn't click. `TapeModelEQ` itself handles
the `SWITCH` parameter's two model-switch behaviors (FADE: parallel-chain crossfade; CLUNK: hard
coefficient swap timed under a mute dip, plus a thump) - every active GEN stage reacts to the same
shared `model`/`switchMode` values each block, so they switch in sync with no cross-stage
coordination needed. `FailureEngine`'s effective intensity is `max(failure01, failAuxValue)`, where
`failAuxValue` comes from an `AuxEnvelope` driven by the `FAIL` aux trigger and shared `RAMP` time;
`TapeStop`/`FilterSweep` are driven by the `STOP`/`FILTER` aux triggers the same way.

Each DSP stage (`Source/DSP/`) is a self-contained class with `prepare(juce::dsp::ProcessSpec)` and a `process(buffer, ...)` method taking plain floats/bools read from the APVTS each block — no DSP stage reads the APVTS directly. All stages have real DSP implemented; the one intentional placeholder is the REVOX B77 tape model's EQ curve (see `TapeModelData.h` — deliberately near-transparent pending measured reference data).

Latency is reported via `saturator.getLatencySamples()` — if a future stage introduces its own latency, `prepareToPlay` needs to sum across stages.

### Parameters

`Source/Parameters.h` is the single source of truth for parameter IDs (`ParamIDs::*`) and the APVTS layout (`createTapeRotParameterLayout()`). `PluginProcessor` caches raw atomic pointers to each parameter in its constructor via `apvts.getRawParameterValue(...)` and reads them fresh every block in `processBlock` — don't call `getRawParameterValue` per-block, and don't add a parameter without adding both the layout entry and the cached pointer.

`failAuxDisplay` is exponentially smoothed per-block in `processBlock` from the FAIL aux envelope's output and exposed via `getFailAuxDisplay()` for the FAIL lamp to poll without touching the audio thread's actual parameter values. `getGenDisplay()`/`getStopSpeedDisplay()` similarly expose live internal state (the smoothed GEN value, the tape-stop ramp speed) for the Scope. These are the only three GUI-facing "derived" display getters — a wow/flutter/failure-amount trio was scaffolded the same way at one point but removed since nothing ever read them and they'd have just mirrored their own knob's static value, not measured anything from the actual audio.

### GUI (`Source/GUI/`)

The entire interface is custom-painted (no JUCE Components with default look beyond sliders) against a **fixed reference canvas of 960x434** defined in `TapeRotTheme.h` (`Layout::canvasWidth/canvasHeight`). All layout constants (positions, radii, font sizes, tracking/letter-spacing) live in `TapeRotTheme.h` alongside the full colour palette — components should pull from `TapeRotTheme::Layout`/`TapeRotTheme::Colour` rather than hardcoding numbers.

All UI text (everything except the Dymo nameplate, see below) uses Inter (`design/inter/`, SIL Open Font License 1.1 — see `design/inter/LICENSE.txt`), embedded as binary data via `sansRegularTypeface()`/`sansBoldTypeface()` in `TapeRotTheme.h` rather than a named system font — this used to be "Helvetica Neue" (macOS-only, and the tracking/kerning constants here are tuned against a specific typeface's metrics, so it needs to be the same file on every platform).

Scaling to the actual window size is handled once, centrally: `PluginEditor::resized()` computes a single uniform scale factor (`window width / referenceWidth`) and applies it as a transform on the whole `TapeRotEditorContent`, with the constrainer locking the aspect ratio. Individual GUI components (`SectionPanel`, `DymoLabel`, knob slider look-and-feel in `TapeRotLookAndFeel`) always draw in the untransformed 960x434 reference space and never need to know about the current window size.

`SectionPanel` does all the static/background painting (bezel, panel, header, section labels/dividers, knob labels, switch labels, failure-dot labels, counter housing, screws, version text) in one Component, driven entirely by `TapeRotTheme::Layout` constants — it has no interactive elements. `TapeRotLookAndFeel::createKnobSlider` builds the actual interactive `juce::Slider` knobs, which `TapeRotEditorContent` positions from `Layout::knobs` and binds to APVTS parameters via `SliderAttachment`.

`design/taperot-interface.svg` is the reference visual mock the theme/layout constants were derived from — check it when adjusting layout numbers or colours. `design/icon/` holds the app icon source (SVG) and exported PNGs at each required size; the plugin/app icon is wired via `ICON_BIG`/`ICON_SMALL` in `CMakeLists.txt` (`juce_add_plugin`), which JUCE turns into a generated `.icns` at build time — don't hand-maintain an `.icns` file.

`DymoLabel` draws the "TAPEROT" nameplate using `design/impact-label/Impact_label_reversed.ttf` (donationware, commercial use confirmed with the author - check license terms before swapping in any other third-party font the same way), embedded as binary data rather than depended on as an installed system font — see Build system below. Its glyphs are placed via `GlyphArrangement` at an explicit baseline computed from actual ink bounds (not `drawText`'s ascent/descent-based centring, which doesn't line up for this hand-drawn display font); the plate itself has intentionally-imperfect "hand-applied label" geometry (rotation, asymmetric hand-cut edges, slight bow, contact shadow) via named `dymo*` constants in `TapeRotTheme.h`.

### Build system

`CMakeLists.txt` fetches JUCE via `FetchContent` (pinned to `8.0.14`) and defines one `juce_add_plugin(TapeRot ...)` target. `FORMATS` and `VST3_COPY_DIR`/`AU_COPY_DIR` are set from an `if(APPLE)`/`else()` block (`TAPEROT_FORMATS`/`TAPEROT_EXTRA_ARGS`) — AU only builds on Apple, and the Windows branch leaves the copy dirs unset so JUCE applies its own correct Windows default rather than a macOS path built on `$ENV{HOME}`. `CMAKE_OSX_DEPLOYMENT_TARGET`/`CMAKE_OSX_ARCHITECTURES` are likewise only set `if(APPLE)`. `PLUGIN_MANUFACTURER_CODE` (`Trot`), `PLUGIN_CODE` (`Rota`), `BUNDLE_ID`, and `COMPANY_NAME` are placeholders per BUILDING.md — treat them as effectively permanent once anything is shipped or automated against them, so confirm before changing. `Tests/` is a separate `juce_add_console_app` target that compiles the DSP `.cpp` files directly (not linked against the plugin target) plus its own Catch2-style test files — new DSP `.cpp` files need to be added to both `target_sources(TapeRot ...)` in the root `CMakeLists.txt` and `target_sources(TapeRotTests ...)` in `Tests/CMakeLists.txt` if you want them covered by tests.

`juce_add_binary_data(TapeRotBinaryData SOURCES design/impact-label/... design/inter/...)` embeds the Dymo-label and Inter font files as C++ byte arrays (`BinaryData::Impact_label_reversed_ttf`/`..._ttfSize`, `BinaryData::InterRegular_ttf`/`InterBold_ttf` — note JUCE's binary-data name-mangling strips hyphens rather than converting them to underscores, so `Inter-Regular.ttf` becomes `InterRegular_ttf`, not `Inter_Regular_ttf`) rather than loading them from disk at runtime; it's linked into both the `TapeRot` plugin target and `TapeRotTests` (anything that includes `TapeRotTheme.h`, which calls `Typeface::createSystemTypefaceFor` on both, needs the link). New embedded assets go in this same `juce_add_binary_data` call, not as raw `target_sources`.

### Platform-specific notes

No exotic macOS APIs exist anywhere in `Source/DSP/` (confirmed by audit — no Objective-C++, no CoreAudio/CoreMIDI/AudioToolbox/Accelerate, no SIMD intrinsics), so the DSP side ports unmodified to both Windows and Linux. The only platform branches anywhere in the codebase are:

- `Source/PluginProcessor.cpp`'s `getUserPresetDirectory()` — `#if JUCE_WINDOWS || JUCE_LINUX` → `<AppData>/<Manufacturer>/<Plugin>/Presets` (JUCE's `userApplicationDataDirectory` resolves this to `%APPDATA%` on Windows and `~/.config` on Linux), else macOS's `~/Library/Audio/Presets/...`.
- `Source/GUI/TapeRotTheme.h`'s `dymoFont()` — per-backend pinned `withHeight` constant for the Dymo-label typeface (`JUCE_WINDOWS`: `18.0f`, measured against DirectWrite; else `29.4f`, measured against CoreText). The `JUCE_LINUX` branch currently reuses the macOS value as an explicitly-flagged placeholder — Linux renders through FreeType, a third backend, and hasn't been measured on real hardware the way Windows was (see the Windows Dymo-sizing fix in git history for the measurement methodology to repeat here).
- the `CMakeLists.txt` items above (`if(APPLE)` gating `FORMATS`/copy dirs/deployment target).

Everything else was already cross-platform-correct (icon generation, `createLegalFileName`, JUCE's own MSVC-aware recommended-flags targets) or has no Windows/Linux equivalent by nature (AU).

`.github/workflows/edge-build.yml` builds and publishes a macOS installer (`.pkg`, ad-hoc signed), a Windows installer (`installer/windows/TapeRot.iss`, built via Inno Setup - pre-installed on GitHub's Windows runners, no setup step needed - unsigned), and a Linux tarball (`.tar.gz` containing the VST3/Standalone plus `installer/linux/README.txt`, unsigned — no Linux equivalent of code-signing to bypass) to the same rolling `edge` GitHub pre-release on every push to `develop`. The Windows installer places the VST3 in `%COMMONPROGRAMFILES%\VST3\` and the Standalone under `{autopf}\TapeRot\` (Program Files), both requiring admin elevation. The Linux `build-linux` CI job runs on `ubuntu-22.04` (not `ubuntu-latest`) for an older glibc baseline and broader binary compatibility with users' distros, and needs the JUCE Linux apt dependency list (ALSA/JACK/FreeType/Fontconfig/X11/GTK/mesa dev packages — see the job or `BUILDING.md` for the full list) installed before configuring.
