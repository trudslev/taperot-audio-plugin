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
cmake -B build -G Xcode
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
auval -v aufx Rota Nfdy                      # full AU validation

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

The panel is a **bitmap**, not vector art. `design/assets/2x/panel_background_2x.png` is blitted whole, and everything printed on it — every label, scale legend, tick, numeral, unit mark and the TAPEROT nameplate — is baked into that plate. Exactly four things are drawn at runtime: the scope trace, the PROGRAM LCD text, the MODEL readout, and the IN/OUT numerals. If you find yourself about to draw a printed word in code, it belongs in the plate instead.

The reference canvas is **1336x679**, defined in `TapeRotTheme.h` (`Layout::canvasWidth/canvasHeight`). Every coordinate in that file is absolute against it, in 1x design pixels, taken from `design/GUI-SPEC.md`. Assets are embedded at 2x only and scaled down for smaller windows (`Layout::assetScale`) — the fascia texture is fine enough that a 1:1 blit to a Retina display resolves to a flat wash. Components pull from `TapeRotTheme::Layout`/`Colour`/`Asset` rather than hardcoding numbers.

Share Tech Mono (`design/fonts/share-tech-mono/`, SIL OFL 1.1) is the **only** typeface needed at runtime, because it is the only text drawn at runtime. Inter and ImpactLabel are gone — all their text is in the plate.

Scaling is handled once, centrally: `PluginEditor::resized()` computes a single uniform scale factor and applies it as a transform on the whole `TapeRotEditorContent`, with the constrainer locking the aspect ratio. Range is 0.5x-2x per BRAND.md's accessibility-lever requirement. Individual components always draw in the untransformed 1336x679 space.

Components, all in `Source/GUI/`:

- `KnobFilmstrip` — a `juce::Slider` that paints one frame of a vertical filmstrip. `frame = round(valueToProportionOfLength(value) * (frames-1))`; the strip sweeps **−120° to +120°**, baked frame by frame. Large caps and small caps are 128 frames; MODEL is 9, indexed directly with no interpolation (NONE plus eight machines — `FilmstripConformanceTests` fails if the strip and `kTapeModels` ever disagree). The sprite is the **cap only**; ticks, numerals and the control's name are in the plate.

  Note `valueToProportionOfLength`, not the raw value: five parameters are skewed (DRIVE and FLUTTER 0.2, LP and HP 0.3, RAMP 0.4), so rotation is `((value − min) / (max − min)) ^ skew` and the printed ticks are placed under those curves. Deriving the frame from the raw value would put the needle nowhere near its mark on those five.

  **`Layout::knobs` holds the sprite's top-left, and the dial centre is that + frameSize/2** — y 386.0 for the large row, 531.6 for the small. Those y values were 7.27px lower until delta v1.0.5: the handoff had measured the centre of the whole knob *element* (dial box plus the control name printed under it), so the name dragged it below the tick arc's real centre. Blitting the cap there put the needle's pivot below the arc it sweeps and the tip landed ~7° past the printed end mark, which reads as the knob showing a small negative value at minimum. If the needle ever looks off again, measure the tick arc's centre before suspecting the sweep angle — a wrong centre compresses measured tick angles symmetrically toward vertical and looks exactly like a sweep mismatch.
- `SpriteButton` — two-state exclusive-select groups (SWITCHING, NOISE BED, HUM, SPREAD). Selecting never toggles off.
- `LampStrip` — GENERATION segments, the FAULT ACTIVITY dots, the momentary FAIL buttons and the scope's FAIL LED. Drains `FailureEngine::popEvents` on a 30 Hz timer and flashes each dot for 260 ms.
- `PitchScope` — the trace, grid and both legend rows.
- `ProgramHeader` — the PROGRAM LCD (bank chip, name, selector menu, name entry), SAVE/DELETE/CANCEL, the MODEL readout and the IN/OUT numerals. See **The Program header** below; it carries more behaviour than anything else in the GUI.
- `ProgramMenuLookAndFeel` — the Program dropdown, painted as an extension of the LCD glass. The menu is the one thing here that can't be a bitmap (its height depends on how many User Programs exist), so it's the only place a JUCE default would otherwise show through.

Several controls deliberately **share** one sprite: `knob_large` serves all seven large knobs, `knob_small` all three small ones, and one lamp serves both the fault dots and the FAIL buttons. The handoff ships those as separate byte-identical files; only one copy of each is embedded (34 sprites, ~9.5 MB, rather than the 50-odd files the bundle contains). `TapeRotTheme.h`'s `Asset` namespace is the role-to-sprite map — if a strip looks "missing", check there before adding a file.

**A full-canvas component must override `hitTest`.** `LampStrip` and `ProgramHeader` both size themselves to the whole canvas so they can draw anywhere on it, and both intercept mouse clicks. `TapeRotEditorContent`'s `toBack()` ordering leaves LampStrip in front of ProgramHeader, so before they were given `hitTest` overrides LampStrip silently swallowed every click on the header and the Program dropdown, SAVE and DELETE were all dead. Each now claims only the pixels it actually handles. Any new overlay component drawn across the panel needs the same treatment.

`design/GUI-SPEC.md` is authoritative for coordinates and `design/Handoff Assembly.dc.html` is the reference build (open it straight from `design/`). `design/plate/buttons/` holds the five per-state Program-button renders, which are the acceptance target for the header — the lighting is a judgement call that no coordinate settles, so check against the image rather than the prose. `design/icon/` holds the icon PNGs; the plugin/app icon is wired via `ICON_BIG`/`ICON_SMALL` in `CMakeLists.txt`, which JUCE turns into a generated `.icns` — don't hand-maintain one. **JUCE builds the `.icns`/`.ico` at *configure* time and the PNGs are not configure dependencies, so after changing icon artwork you must re-run `cmake -B build` or the previous icon ships silently.**

The nameplate is baked rather than drawn live, deliberately. It used to be drawn in ImpactLabel, whose vertical metrics are read very differently by each text backend (~0.61x cap-ink through CoreText vs ~1.00x through DirectWrite), forcing a pinned per-backend height constant with an unverified Linux placeholder. Baking made all three platforms render identical artwork and deleted that branch. The same reasoning is why nothing else on this panel is drawn live either.

### The Program header

**The header band is 34px at y 63, and TapeRot is the only casting where it got smaller.** The LCD,
both Program buttons and both meter wells share that row — the meters used to be 42 tall at y 47,
two pixels taller than their own row-mates, which is the drift the suite audit found in four
castings and which nobody had ever caught by eye. 34 is BRAND.md's suite figure rather than this
panel's: the castings are differently-sized units, not scales of one design.

Verified against the v1.0.8 plate rather than the spec's table — the LCD glass, the IN well and the
OUT well all measure **64..96** there, which is the 32px content box, so the border-box is 63..97
for all three. The SAVE and DELETE positions are empty in the plate, as they have been since delta
v1.0.7, so both buttons are entirely sprite.

**Each button is one three-frame strip and the frame order is fixed.** Frame 0 both legends dark,
1 top lit, 2 bottom lit. Both legends lit is not a state and is deliberately not exported: the two
functions are mutually exclusive, so a fourth face could only ever indicate a bug. **Frame 0 is
inert in code as well as in appearance** — a button showing both legends dark must not act when
clicked, or the backlight is claiming something the control contradicts.

That replaced `saveButton(bool)` plus a `DeleteFace` enum whose third value was a *relabel*: the
sprite reading CANCEL where its neighbour read DELETE. A printed panel legend cannot rewrite itself,
which is exactly what the second legend exists to solve — and the old two-face SAVE had no STORE at
all, so naming left it reading SAVE while acting as STORE.

**Read the state matrix by row, not by button.** Naming overrides both resting legends: while a name
is being typed SAVE and DELETE are dark even on an edited User Program, because nothing can be saved
or deleted until the name is committed or abandoned.



Four behaviours share the one LCD, which is why `ProgramHeader` is the busiest component here.

**Selecting.** Clicking the glass opens the Program menu — Factory section, then User if any exist, current one marked with a lit amber pip. Selection goes through `setCurrentProgram`, which defers via the processor's `AsyncUpdater` because a host can call it off the message thread, so the repaint waits for the apply rather than happening in the callback.

**Naming.** SAVE opens an entry field in the glass rather than saving immediately: characters uppercase as they arrive, a block caret (U+2588) blinks at 1 s / 50 % duty, **Enter commits, Escape cancels**, and DELETE wears the CANCEL sprite and does the same. `TapeRotEditorContent` already repaints this component at 60 Hz for the meters, so the caret needs no timer of its own. An empty name falls back to `USER PROGRAM` **inside `saveUserProgram`**, not in the caller, so no future caller can write a dotfile. Cancelling must never touch a parameter — knob values tweaked but not yet saved have to survive it.

**SAVE gating.** `TapeRotAudioProcessor::isProgramModified()` compares the live parameters against a snapshot taken whenever a Program is applied or a session restored, so SAVE stays dark until something has actually moved. Two decisions worth keeping: the snapshot is taken from the **live APVTS**, not rebuilt from the Program's definition, so `applyFactoryProgram` stays the single description of what a Program sets with no second copy to drift; and STOP/FILTER/FAIL are excluded from it, because they're momentary and never saved, so holding one must not light SAVE up. `setStateInformation` can arrive on any thread while the GUI polls on the message thread — hence the spin lock.

**Parameter takeover.** While a control is dragged the LCD shows `PARAMETER: value unit`, reverting 1.1 s after release. Guarded on the control's own drag state, because a `SliderAttachment` also fires when a Program is applied and on every host automation step — without the guard the display latches onto whichever parameter was written last and flickers for the length of a song. It also stands down entirely during naming; the glass belongs to the name field until it commits or cancels.

The three header buttons are sprites, not baked (delta v1.0.7 cleared both frames from the plate). **Their hit areas are the plate rects, not the sprite rects** — the sprites carry a 3 px shadow bleed that must not be clickable.

### Metering taps

The scope is fed by two read-only taps, both single-producer/single-consumer, lock-free, non-allocating, and both **drop rather than block** when full — an editor that is closed, stalled or repainting slowly costs the audio thread nothing.

- `FailureEngine`'s event FIFO (`pushEvent`/`popEvents`) drives the FAULT ACTIVITY dots. Its four `FailureEventType` values map 1:1 to DRP/SNG/CRK/WBL.
- `PitchDeviationMeter` carries the realised pitch deviation. `WowFlutter::process` takes an optional `float* deviationCentsAccum`; pitch deviation is the *rate of change* of the delay line, `cents = -1200/ln2 * d(delay)/dn`, measured on channel 0 only (channel 1 carries a phase offset that would partially cancel it) and accumulated across every active GEN stage. Decimation takes each window's **extreme**, not a point sample — flutter is spiky and point-sampling walks straight past a transient.

These measure the actual audio, never a knob. A wow/flutter/failure display trio was scaffolded once before and removed because it only mirrored its own knob's static value; `MeteringTests` asserts the difference (flutter must out-deviate wow at equal depth, which is only true of the derivative).

### Build system

`CMakeLists.txt` fetches JUCE via `FetchContent` (pinned to `8.0.14`) and defines one `juce_add_plugin(TapeRot ...)` target. `FORMATS` and `VST3_COPY_DIR`/`AU_COPY_DIR` are set from an `if(APPLE)`/`else()` block (`TAPEROT_FORMATS`/`TAPEROT_EXTRA_ARGS`) — AU only builds on Apple, and the Windows branch leaves the copy dirs unset so JUCE applies its own correct Windows default rather than a macOS path built on `$ENV{HOME}`. `CMAKE_OSX_DEPLOYMENT_TARGET`/`CMAKE_OSX_ARCHITECTURES` are likewise only set `if(APPLE)`. `PLUGIN_MANUFACTURER_CODE` (`Nfdy`, shared by every Neon Foundry plugin), `PLUGIN_CODE` (`Rota`), `BUNDLE_ID` (`com.neonfoundry.taperot`) and `COMPANY_NAME` (`Neon Foundry`) are settled, not placeholders — changing them again breaks saved projects in both AU and VST3, since JUCE derives the VST3 class ID from the manufacturer and plugin codes together. `Tests/` is a separate `juce_add_console_app` target that compiles the DSP `.cpp` files directly (not linked against the plugin target) plus its own Catch2-style test files — new DSP `.cpp` files need to be added to both `target_sources(TapeRot ...)` in the root `CMakeLists.txt` and `target_sources(TapeRotTests ...)` in `Tests/CMakeLists.txt` if you want them covered by tests.

`juce_add_binary_data(TapeRotBinaryData SOURCES design/assets/2x/... design/fonts/...)` embeds the deduplicated 2x sprite set and Share Tech Mono as C++ byte arrays rather than loading them from disk at runtime. Note JUCE's binary-data name-mangling strips non-alphanumerics rather than converting them to underscores, so `ShareTechMono-Regular.ttf` becomes `ShareTechMonoRegular_ttf`, not `ShareTechMono_Regular_ttf`. It's linked into both the `TapeRot` plugin target and `TapeRotTests` (anything that includes `TapeRotTheme.h` needs the link — `FilmstripConformanceTests` reads the real sprites). New embedded assets go in this same `juce_add_binary_data` call, not as raw `target_sources`.

Unlike the icon, BinaryData **is** a build-time dependency and tracks correctly — changing a sprite and rebuilding picks it up without reconfiguring.

### Platform-specific notes

No exotic macOS APIs exist anywhere in `Source/DSP/` (confirmed by audit — no Objective-C++, no CoreAudio/CoreMIDI/AudioToolbox/Accelerate, no SIMD intrinsics), so the DSP side ports unmodified to both Windows and Linux. The only platform branches anywhere in the codebase are:

- the `CMakeLists.txt` items above (`if(APPLE)` gating `FORMATS`/copy dirs/deployment target).

`getUserProgramDirectory()` **used to** be a platform branch here and no longer is. User Programs now
live at `<AppData>/<Manufacturer>/<Plugin>/Programs` on all three platforms, via JUCE's
`userApplicationDataDirectory` — `~/Library/Application Support` on macOS, `%APPDATA%` on Windows,
`~/.config` on Linux. That segment is JUCE's and must never be hard-coded; a shared literal would be
wrong on two of the three.

The macOS branch used to point at `~/Library/Audio/Presets/<Manufacturer>/<Plugin>`, justified as
Apple's shared convention directory. **That reasoning was wrong**, and is worth recording so it
isn't reinstated: `~/Library/Audio/Presets` is where the **AU preset format** lives — `.aupreset`
files the AU system itself scans, reads and writes. A `.taperotprogram` sitting there was never
going to be discovered by anything, so the path bought no interoperability while asking Apple's
folder to hold a format it does not understand. All six castings now agree; Elmer had it right
first.

Everything else was already cross-platform-correct (icon generation, `createLegalFileName`, JUCE's own MSVC-aware recommended-flags targets) or has no Windows/Linux equivalent by nature (AU).

`.github/workflows/edge-build.yml` builds and publishes a macOS installer (`.pkg`, ad-hoc signed), a Windows installer (`installer/windows/TapeRot.iss`, built via Inno Setup - pre-installed on GitHub's Windows runners, no setup step needed - unsigned), and a Linux tarball (`.tar.gz` containing the VST3/Standalone plus `installer/linux/README.txt`, unsigned — no Linux equivalent of code-signing to bypass) to the same rolling `edge` GitHub pre-release on every push to `develop`. The Windows installer places the VST3 in `%COMMONPROGRAMFILES%\VST3\` and the Standalone under `{autopf}\TapeRot\` (Program Files), both requiring admin elevation. The Linux `build-linux` CI job runs on `ubuntu-22.04` (not `ubuntu-latest`) for an older glibc baseline and broader binary compatibility with users' distros, and needs the JUCE Linux apt dependency list (ALSA/JACK/FreeType/Fontconfig/X11/GTK/mesa dev packages — see the job or `BUILDING.md` for the full list) installed before configuring.
