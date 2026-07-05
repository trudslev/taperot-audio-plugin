# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

Configure once (JUCE 8.0.14 is fetched automatically via CMake `FetchContent`, no local checkout needed):

```sh
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
```

Re-run the configure step whenever `CMakeLists.txt` changes (new sources, new `juce_add_plugin` args, etc.) — a plain rebuild won't pick those up.

Build (produces AU, VST3, and Standalone; auto-installs AU/VST3 to `~/Library/Audio/Plug-Ins/...`):

```sh
cmake --build build --config Release
```

Run the DSP unit tests (Catch2-style, built as a console app target `TapeRotTests`):

```sh
./build/Tests/TapeRotTests_artefacts/Release/TapeRotTests
```

To run a single test, pass its name/tag as an argument to that binary (Catch2 CLI conventions — e.g. `./build/Tests/TapeRotTests_artefacts/Release/TapeRotTests "[saturator]"`).

Validate plugin formats after building:

```sh
auval -a | grep -i tapero                    # confirm AU registration + 4-char codes
auval -v aufx Rota Trot                      # full AU validation

/Applications/pluginval.app/Contents/MacOS/pluginval \
    --strictness-level 8 \
    --validate ~/Library/Audio/Plug-Ins/VST3/TapeRot.vst3
```

If Logic Pro doesn't pick up a freshly built AU: Audio Units Manager → "Reset & Rescan Selection", or restart Logic. If a rebuilt app/plugin icon doesn't refresh, LaunchServices/Dock/Finder are caching by bundle path — `touch` the bundle, re-run `lsregister -f <bundle>`, then `killall Dock Finder`.

See [BUILDING.md](BUILDING.md) for requirements (Xcode full install, CMake 3.24+, pluginval).

## Architecture

### Signal chain (fixed order, all in `PluginProcessor::processBlock`)

```
Saturator -> WowFlutter -> TapeModelEQ -> NoiseGenerator -> FailureEngine -> StereoSpread -> OutputStage
```

Each DSP stage (`Source/DSP/`) is a self-contained class with `prepare(juce::dsp::ProcessSpec)` and a `process(buffer, ...)` method taking plain floats/bools read from the APVTS each block — no DSP stage reads the APVTS directly. **Only Saturator, WowFlutter, and TapeModelEQ have real DSP implemented**; NoiseGenerator, FailureEngine, StereoSpread, and OutputStage are currently passthrough stubs with their parameters already wired end-to-end (APVTS -> processor -> stage call), so adding real DSP to them means editing the stage class only, not the wiring.

Latency is reported via `saturator.getLatencySamples()` — if a future stage introduces its own latency, `prepareToPlay` needs to sum across stages.

### Parameters

`Source/Parameters.h` is the single source of truth for parameter IDs (`ParamIDs::*`) and the APVTS layout (`createTapeRotParameterLayout()`). `PluginProcessor` caches raw atomic pointers to each parameter in its constructor via `apvts.getRawParameterValue(...)` and reads them fresh every block in `processBlock` — don't call `getRawParameterValue` per-block, and don't add a parameter without adding both the layout entry and the cached pointer.

Three atomics (`wowDisplay`, `flutterDisplay`, `failureDisplay`) are exponentially smoothed per-block in `processBlock` and exposed via `getWowDisplay()`/`getFlutterDisplay()`/`getFailureDisplay()` for the editor to poll (e.g. for meter/scope drawing) without touching the audio thread's actual parameter values.

### GUI (`Source/GUI/`)

The entire interface is custom-painted (no JUCE Components with default look beyond sliders) against a **fixed reference canvas of 960x400** defined in `TapeRotTheme.h` (`Layout::canvasWidth/canvasHeight`). All layout constants (positions, radii, font sizes, tracking/letter-spacing) live in `TapeRotTheme.h` alongside the full colour palette — components should pull from `TapeRotTheme::Layout`/`TapeRotTheme::Colour` rather than hardcoding numbers.

Scaling to the actual window size is handled once, centrally: `PluginEditor::resized()` computes a single uniform scale factor (`window width / referenceWidth`) and applies it as a transform on the whole `TapeRotEditorContent`, with the constrainer locking the aspect ratio. Individual GUI components (`SectionPanel`, `DymoLabel`, knob slider look-and-feel in `TapeRotLookAndFeel`) always draw in the untransformed 960x400 reference space and never need to know about the current window size.

`SectionPanel` does all the static/background painting (bezel, panel, header, section labels/dividers, knob labels, switch labels, failure-dot labels, counter housing, screws, version text) in one Component, driven entirely by `TapeRotTheme::Layout` constants — it has no interactive elements. `TapeRotLookAndFeel::createKnobSlider` builds the actual interactive `juce::Slider` knobs, which `TapeRotEditorContent` positions from `Layout::knobs` and binds to APVTS parameters via `SliderAttachment`.

`design/taperot-interface.svg` is the reference visual mock the theme/layout constants were derived from — check it when adjusting layout numbers or colours. `design/icon/` holds the app icon source (SVG) and exported PNGs at each required size; the plugin/app icon is wired via `ICON_BIG`/`ICON_SMALL` in `CMakeLists.txt` (`juce_add_plugin`), which JUCE turns into a generated `.icns` at build time — don't hand-maintain an `.icns` file.

### Build system

`CMakeLists.txt` fetches JUCE via `FetchContent` (pinned to `8.0.14`) and defines one `juce_add_plugin(TapeRot ...)` target producing AU + VST3 + Standalone from the same source list. `PLUGIN_MANUFACTURER_CODE` (`Trot`), `PLUGIN_CODE` (`Rota`), `BUNDLE_ID`, and `COMPANY_NAME` are placeholders per BUILDING.md — treat them as effectively permanent once anything is shipped or automated against them, so confirm before changing. `Tests/` is a separate `juce_add_console_app` target that compiles the DSP `.cpp` files directly (not linked against the plugin target) plus its own Catch2-style test files — new DSP `.cpp` files need to be added to both `target_sources(TapeRot ...)` in the root `CMakeLists.txt` and `target_sources(TapeRotTests ...)` in `Tests/CMakeLists.txt` if you want them covered by tests.
