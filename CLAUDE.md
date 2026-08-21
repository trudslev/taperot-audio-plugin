# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## RESUME POINT — the header has a measured baseline, and it is what a panel move fails against

**Verified `23da122` on 2026-08-17: this casting's header draws exactly where its own constants say.**
Not read — captured from the Release standalone and measured off the pixels.

Why it is written here rather than left in a session report: the value of a baseline is entirely in
being read by whoever moves the panel next, and a figure that lives somewhere the mover does not
open is worth nothing.

**What was measured**, band at y 63, height 34, canvas 1336 x 679 — **the pre-rewrite panel**,
kept as the baseline it was taken as. The canvas is 1340 x 790 and the band is the shared part's as
of 2026-08-21; see the GUI section:

| Element | Constants | Measured |
|---|---|---|
| LCD | `lcdDivider` 491 splits it | 417.0 .. 884.0, split at 489.5/491.0 |
| SAVE | `saveHitArea` 896.2 .. 972.2 | 897.5 .. 995.0 (glyph-broken) |
| DELETE | `deleteHitArea` 982.2 .. 1058.2 | 1044.5 .. 1055.5 (glyph-broken) |
| meter wells | — | 1132.0 .. 1210.5, 1225.0 .. 1303.5 |

**What this is NOT.** This casting references `nf::HeaderGeometry` **nowhere**, so it is on its own
canvas and its own layout, and none of the figures above is expected to match the shared part. The
baseline says *internally consistent*, not *conformant*.

**The defect it exists to catch** was found in Chorus-60 on 2026-08-17: that casting's header pass
aliased its LCD to the shared part and left SAVE, DELETE and both meter wells as literals from the
previous canvas — **29 px right and 29 px down** — and nothing could see it, because the plate baked
those faces and the only symptom was text centred inside a box nobody drew. It surfaced the moment
the material had to be painted from those rects.

**So when this casting moves: alias every band figure in one edit, then re-measure against the table
above.** A rect that moves and a rect that does not are indistinguishable in a diff and obvious in a
measurement. And note that **a literal which happens to agree with core is indistinguishable from an
alias by reading** — Reflect-84 held four such literals, one of them 2 px off §4's shared descriptor
anchor, in the casting whose editor had been declared conformant.

---

## THE SUITE'S **FIVE** RED ARMS HERE — filed against the sweep's existing rows, 2026-08-18

**Re-run against a freshly built binary on 2026-08-20, and all four documented figures reproduce to
NINE SIGNIFICANT FIGURES.** They were re-verified because this casting's test binary turned out to be
four days old — a cache variable pointing at a deleted scratch directory made every configure fail
while every build reported success. The re-run is what turns them from a result into a result that
carries:

| Arm | Filed 2026-08-18 | Fresh binary, core `fcd8268` |
|---|---|---|
| `NoiseSource` at character 1 | 3.72529e-09 | **3.72529e-09** |
| 128 vs 64 | 0.000200262 @ 0 | **0.000200262 @ 0** |
| 511 vs 64 | 0.001022242 @ 0 | **0.001022242 @ 0** |
| 2048 vs 64 | 0.001926094 @ 0 | **0.001926094 @ 0** |

**And the heading said four when the suite reports five.** The fifth is
*Invariance / the block-size rows — ToneFilters' LP ramp, armed on EVERY prepare*, which refutes
that candidate: 20 kHz 0.001926094 against 1 kHz 0.002924729, so the divergence does not grow with
the smoother's travel. `git show 47efda3:Tests/InvarianceTests.cpp` has it, so it was failing on the
day the four were filed and was simply not counted — the shape this suite records as *a list reads
as complete*, arriving in a list of failures.

**What the stale binary did and did not invalidate**, because the first answer given was too strong.
`git diff dc92bd2 c4eeae8 -- Source/ Tests/ CMakeLists.txt` is **empty**: nothing but `CLAUDE.md` and
twelve files under `design/` changed in the whole window. The binary was stale in its DATE and
current in its CONTENT, so the window's results were run against the right source and stand.

The one thing that genuinely did not carry is the repin claim, and it is now stronger rather than
withdrawn. Core moved `3feeead → fcd8268` on 2026-08-17 and the argument offered was static — 0 of
779 compiler dependency files mention `nf/HeaderPart.h`, which core's delta is confined to. That
argument was sound and did not depend on the binary. But the sentence saying this casting's `_deps`
*"followed it a day later"* was not true: the configure that would have re-fetched had been failing
since 2026-08-16, so the binary was still linking the old core. **The fresh binary links `fcd8268`
and produces identical figures**, so the claim is confirmed by measurement now instead of by
inference.

---

### The original entry, unchanged

**These are open findings expressed as failing assertions, not regressions.** The suite reports
`TESTS FAILED (exit 1)` and has been meant to: each arm asserts a property the sweep established is
not held, so a green run here would mean an arm had been relaxed.

**They do not come from the core repin.** Core moved `3feeead → fcd8268` on 2026-08-17 and this
casting's `_deps` followed it a day later, which raised the question. Core's delta touches exactly
one public header — `nf/HeaderPart.h` — and **0 of this casting's 779 compiler dependency files
mention it.** That is the compiler's own record rather than an argument from the diff, and it is the
measurement that was available: rebuilding against the old pin is blocked by core's own staleness
check, which is fatal by design and whose only opt-out covers an unreachable remote.

| Arm | Figure | Files against |
|---|---|---|
| `NoiseSource` diverges with block size at character 1 | 3.72529e-09 | **the block-coupling class, RATE form** — a new member |
| Block size 128 vs 64, defaults | 0.000200262, sample 0 | the NOISE-path transient, **localised-not-explained** |
| Block size 511 vs 64, defaults | 0.001022242, sample 0 | same |
| Block size 2048 vs 64, defaults | 0.001926094, sample 0 | same |

### The three block-size rows are NOT the sweep's recorded numbers, and that is the finding

The sweep recorded **0.009630475** at 2048 and slices of 0.009630 / 0.007679 / 0.002036 across the
first three 8 ms windows, then exactly zero from 24 ms. This arm reports **0.001926094** at 2048 —
a factor of five apart.

**They are not comparable, because the configurations differ.** The recorded row states its own:
*NOISE at 100, warmed*. This arm runs at **default parameters**, warmed. A smaller NOISE gives a
smaller divergence, and five-fold is unremarkable for that.

So the correct filing is neither "the same numbers" nor "a different defect". It is the
reproducibility rule arriving in a comparison rather than in a measurement: **a figure is a property
of a processor IN A CONFIGURATION, and two figures measured in different ones cannot be equal or
unequal — only unrelated.** What is comparable is the signature, and it matches on every axis
available: first at sample 0, monotonic in block size, and the 64-against-64 self-comparison exact.

**What would make them comparable is one run**, driving this arm at NOISE 100. That is worth doing
when the row is next opened; it is not worth doing to answer the repin question, which the
dependency records already settled.

### The `NoiseSource` arm is a new member of a recorded class

Its own comment connects it to the catalogued *stored copy of a selection, compared per block* table
— gatecrasher `currentAlgorithm`, reflect-84 `currentAlgorithm`, taperot `TapeModelEQ::activeModelIndex`
— and it is a fourth member. But **the symptom is the RATE form, not the spurious-first-block form**:
the character crossfade is *started* per prepare and *stepped* per block, so its duration in seconds
moves with the buffer size. That is `LfoBank`'s pre-stage-1a defect in a crossfade rather than in a
generator, which is a form the block-coupling table did not have.

At character 0 it cannot fire — requested equals the constructed value, so no crossfade starts — and
the arm says so in place. A probe at the one value that cannot distinguish the hypothesis is the
mistake this sweep already made once with a pre-delay at 0.

## THE PANEL PROTOTYPE RENDERS THE WORDMARK IN A FALLBACK, AND ALWAYS WILL

`design/TapeRot MT-77 Panel.dc.html` declares `@font-face … url('fonts/ImpactLabelReversed.ttf')`.
That file is not there and **must never be**: `design/fonts/ABSENT.md` records the face as
donationware and not embeddable, so the letterforms ship as artwork — `assets/taperot-wordmark.png`,
694 × 150 — and the font does not. Same file ends with *"substituting a face here moves every
measurement taken from the nameplate."* **The prototype substitutes one on every render.**

So this is not a missing file to chase. It is a declaration that guarantees a fallback, and the
declaration is what makes it read as an oversight rather than the decision it is. **No width, glyph
or letter position may be taken off that prototype's nameplate** — the wordmark PNG is the artefact
with the real letterforms, and it is the one to measure.

Chorus-60 has the same symptom for the opposite reason: its face is licensed, embeddable and simply
in the wrong directory. Both are `design-asks/prototype-font-paths-do-not-resolve.md`; only that one
closes by moving a file.

    python3 ../tools/enumerate_prototype.py "design/TapeRot MT-77 Panel.dc.html" --canvas 1340x790

---

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

### GUI (`Source/GUI/`) — REWRITTEN 2026-08-21, and the asset model inverted

**The panel is code-drawn. Exactly one bitmap ships.** Call 5 retired the plate and the filmstrips
together: every knob, cap, pointer, tick, numeral, unit, label, shoe, lamp, divider, heading, well
and readout on this panel is a draw call. The wordmark is the only artwork, and only because
Impact Label Reversed is donationware that cannot be embedded (§9).

**That is the exact opposite of what this section said until the rewrite**, and the old text is worth
knowing about because the failure mode inverted with it. It read:

> The panel is a **bitmap**, not vector art. `design/assets/2x/panel_background_2x.png` is blitted
> whole, and everything printed on it — every label, scale legend, tick, numeral, unit mark and the
> TAPEROT nameplate — is baked into that plate. Exactly four things are drawn at runtime… If you
> find yourself about to draw a printed word in code, it belongs in the plate instead.

Every word was true of revision 1. While the printed layer was baked, the hazard was
**double-printing** — a runtime draw landing on baked ink at a one-pixel offset, which is *visible*.
Now the same element fails by being **absent**, and the panel merely looks emptier than the render.
Chorus-60 made this transition first and its hand-authored enumeration of what the plate had carried
came out **thirteen rows short**, every row in it ink and every missing row material.

**So this panel's object list was DERIVED, not authored.** `tools/enumerate_prototype.py` over
`design/TapeRot MT-77 Panel.dc.html` reports **326 objects — 199 material, 129 ink, in 17 type
roles**, and that enumeration is what the rewrite was built against. Re-derive it rather than
extending a list by hand; a list cannot record the walls of the room it was written in.

The canvas is **1340 × 790** (`Layout::canvasWidth/canvasHeight`). §10 item 1: call 1 cost this
casting **4 px**, 1336 → 1340, the smallest move in the suite, because its band was already close to
the shared part's. Coordinates are absolute against it in 1× logical pixels.

#### The components

| | |
|---|---|
| `TapeRotPanelBackground` | fascia, bezel, four thumbscrews, five dividers, six section headings, the footer row. No timer — it repaints on nothing |
| `KnobComponent` | one control: sweep arc, tick ring, numerals, unit, cap, pointer, label. **Renamed from `KnobFilmstrip` and the rename is the point** — Chorus-60 shipped a `KnobFilmstripComponent` that drew no filmstrip for long enough that a performance hunt went looking for a sheet blit that did not exist |
| `ShoeSwitch` | §4B's two-state shoe, 128 × 32 in two 64 halves. SWITCHING, HUM, SPREAD |
| `LampButtonGroup` | §5.3's Ø26 lamp buttons. FAIL (momentary), NOISE BED (exclusive), FAULT ACTIVITY (toggles) |
| `GenerationLadder` | §5.4's eight stages |
| `PitchScope` | §4's 1308 × 164 well, its trace and four corner readouts |
| `MachineReadout` | §3.3's 134 × 27 cell — **this is MODEL's label**, which is why that ring carries no numerals |
| `ProgramHeader` | the shared header part: block, nameplate, LCD, SAVE/DELETE, both meter wells |

`SpriteButton`, `LampStrip` and `KnobFilmstrip` are **deleted**, not adapted. A sprite path left in
place is a second way to draw a control that nothing exercises.

#### The ring is drawn from the parameter, never from stored angles

`KnobComponent` computes every mark's angle from `getNormalisableRange()` — which
`SliderParameterAttachment` copies off the parameter, so it is the taper that positions the pointer.
That makes BRAND.md's printed-scale defect **unreachable by construction** rather than checked for:
change a skew and the numerals move with the pointer, because they are the same computation.

`Marks` stores **values**, never rotation fractions. Converting all 60 of the delivered prototype's
fractions back through the ranges returns clean round numbers — 1000 / 1200 / 1500 / 2000 / 3000 …
for LP, a 1-1.5-2-3-5-7 decade series for HP and RAMP — which is the evidence they were authored as
values and published as derived output.

**WOW is the one ring that does not match the prototype, deliberately.** Its `pct` table is shared
with NOISE, FAILURE and MIX, which are linear; WOW is **skew 0.3** for a reason `Parameters.h`
argues at length. Drawn at even angles, the printed 50 would put the pointer at **9.92 %**.
`PrintedScaleTests` pins both the correct angles and the prototype's, with the divergence measured;
`design-asks/taperot-wow-ring.md` asks which five numerals to print.

#### Caching, and the two mistakes made getting it right

Every component that repaints caches its static half and counts its rebuilds — **a cache with no
rebuild counter is a cache nobody has checked.** Two errors on the way, both caught by measurement
rather than by reading:

- **`ProgramHeader` cached at CANVAS size.** It spans the canvas so the Program list can anchor and
  so `hitTest` can claim three scattered cells, but it inks a 1308 × 104 strip. The first version
  allocated 1340 × 790 at device scale — 16.9 MB at 2× — and blitted it 60 times a second to deliver
  two meter numerals. Root `CLAUDE.md` records that exact trade from Chorus-60's ModScope in as many
  words, and it was made here anyway. Narrowing it moved the CPU cell 11.95 → 11.67 %, which is
  **a refutation**: the blit was not the cost.
- **`refresh()` was an unconditional full repaint.** On a canvas-spanning component that invalidates
  every child under it. It now repaints the block only when the cache key moves and the two meter
  wells otherwise.

The editor cell sits around **13 % of a core** where the old plate-blitting panel ranged 8.8–15.7 %
depending on block size. `cpu-baseline.json` predates the rewrite and wants re-taking.

#### Type, and the one substitution

§8's thirteen roles are in `Type`, each a **CSS px size with a pinned line box** (call 4). Sizes go
through `withPointHeight`, never `withHeight` — a spec px passed to `withHeight` renders visibly
small, and root `CLAUDE.md` records two castings whose menu type is 11–23 % under nominal for exactly
that reason, one below the functional floor.

**§8 asks for Barlow Condensed 500 for scale numerals and no bundle has delivered a 500 weight to any
casting.** SemiBold stands in, said out loud at the constant rather than silently — Gatecrasher's
rewrite made the identical substitution without recording it.
`design-asks/barlow-condensed-500.md` asks for the cut.

Every non-ASCII glyph is built from its **codepoint**: `juce::String`'s `const char*` constructor
decodes **Latin-1, not UTF-8**. The enumeration reports exactly three above-ASCII codepoints on this
panel — U+00B1, U+00B7, U+2212 — and no whitespace other than U+0020.

#### Contrast

**This casting is declared to `tools/check_contrast.py` for the first time**, and it declares **no
plate** — every ground is a colour constant, resolved by name, so nothing is sampled and nothing can
drift between an asset and the code drawing over it. Ten roles, all clearing their floors and
matching §6's stated figures exactly, including the retired `#b0a695` at 6.03 that §6 cites as the
reason the model line moved to `#ccc1a6`.

#### Two things that bite

**A full-canvas component must override `hitTest`.** `ProgramHeader` sizes itself to the whole canvas
so it can draw the block and anchor the list, and claims only the LCD and the two Program buttons.
Before that override existed, TapeRot's dropdown, SAVE and DELETE were all completely dead and had
been signed off twice. Any new overlay drawn across the panel needs the same treatment.

**Scaling is handled once**, in `PluginEditor::resized()` — one uniform transform on the whole
`TapeRotEditorContent`, aspect locked, 0.5×–2× per BRAND.md's accessibility lever. Components always
draw in the untransformed 1340 × 790 space.

### The Program header

**The whole header is `nf::HeaderGeometry` now, aliased and never transcribed.** §10 item 5 replaced
it with the shared part; the band is **34 at y 61**, the block 1308 × 104 at (16, 16), and the LCD,
both Program buttons and both meter wells take their rects from core. `EditorWiringTests` compares
all six cells against it.

**There are no numbers in `TapeRotTheme::Header`, deliberately.** Chorus-60's header pass aliased its
LCD and left SAVE, DELETE and both meter wells as literals from the previous canvas — **29 px right
and 29 px down** — invisible for as long as the plate baked those faces, because the only symptom was
text centred inside a box nobody drew. *A literal that happens to agree with core is
indistinguishable from an alias by reading.*

What that arm can and cannot do is worth stating: a derivation and a literal are indistinguishable at
runtime **while they agree**, so it catches **divergence**, not a re-typed constant. That is the
whole window in which the two differ at all.

**The nameplate stack is 30 + 44 + 4 = 78**, landing on the shared descriptor anchor, and getting
there settled a row of `design-asks/header-nameplate-offsets.md`. Three sources disagreed: §4's
published table says the plate is 38 (landing on 72, six short), the delivered prototype draws the
descriptor at 84 (six past), and the **delivered cut** says 44. The cut wins because it is the only
one with a file behind it — 694 × 150 at 3× is exactly a 230.2 × 44 strip rotated −1.5°, and §9's own
construction (a 40 px line box with 2 × 18 padding) independently gives 44.
`Header`'s `static_assert` pins it.

**Both Program buttons carry two legends, drawn, and the state matrix is §7.1's.** SAVE/STORE and
DELETE/CANCEL are positions rather than states of one word — BRAND.md forbids a control that
relabels itself, because that is a control the player has to read before pressing. **Both legends
dark is inert in code as well as in appearance**: a button showing nothing lit must not act when
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

**Naming.** SAVE opens an entry field in the glass rather than saving immediately: characters uppercase as they arrive, a block caret (U+2588) blinks at 1 s / 50 % duty, **Enter commits, Escape cancels**, and DELETE's CANCEL legend lights and does the same. `TapeRotEditorContent` already repaints this component at 60 Hz for the meters, so the caret needs no timer of its own. An empty name falls back to `TAKE n` inside `nf::UserProgramStore`, so no caller can write a dotfile — and trimming, upper-casing and the 25-character cap now apply on every save path, not only to the keystrokes typed here. It was `USER PROGRAM` until the suite settled on one fallback; six castings had five different ones. Cancelling must never touch a parameter — knob values tweaked but not yet saved have to survive it.

**SAVE gating.** `TapeRotAudioProcessor::isProgramModified()` compares the live parameters against an `nf::ParameterSnapshot` taken whenever a Program is applied or a session restored, so SAVE stays dark until something has actually moved. Two decisions worth keeping: the snapshot is taken from the **live APVTS**, not rebuilt from the Program's definition, so `applyFactoryProgram` stays the single description of what a Program sets with no second copy to drift; and STOP/FILTER/FAIL are excluded from it, because they're momentary and never saved, so holding one must not light SAVE up.

That exclusion is stated as `isMomentaryTrigger` — a predicate — rather than as the explicit 20-entry inclusion list it replaced. The list happened to cover the APVTS exactly (20 + 3 = 23), so a parameter added later without a matching line would silently have gone unchecked, with SAVE staying dark while it moved. The two forms are equivalent today; only the predicate stays equivalent.

`setStateInformation` can arrive on any thread while the GUI polls on the message thread — the spin lock that used to guard this is inside `nf::ParameterSnapshot` now, so all six castings get it. Four of them had that read and written across threads with nothing at all.

**Parameter takeover.** While a control is dragged the LCD shows `PARAMETER: value unit`, built by `nf::describeParameter` and reverting **900 ms** after release — `nf::ReadoutFormat::revertMs`, where this panel carried 1100. The suite ran 800 / 900 / 1100 / 1200 under three constant names with no spec justifying any of them.

This panel set `ValueCase::wordsOnly` until 2026-08-13, when the designers ruled that **case belongs at the source, never at a display site** — a choice that should read `CLUNK` is authored that way in `Parameters.h`, so the LCD and the host's automation lane print one string. `ValueCase` is deleted from core, and its line was removed from `TapeRotTheme.h` to restore the build. **That deletion is a compile fix, not the ruling: the caps re-authoring in `Parameters.h` is still outstanding here**, so the readout currently prints names and choice values as authored. See the root `../CLAUDE.md` under "Case belongs at the source" for the outstanding list and the byte-identical acceptance bar. Guarded on the control's own drag state, because a `SliderAttachment` also fires when a Program is applied and on every host automation step — without the guard the display latches onto whichever parameter was written last and flickers for the length of a song. It also stands down entirely during naming; the glass belongs to the name field until it commits or cancels.

The three header buttons are **drawn**, and the hit rect IS the drawn cell. This paragraph used to
read *"the three header buttons are sprites, not baked… **their hit areas are the plate rects, not
the sprite rects** — the sprites carry a 3 px shadow bleed that must not be clickable"*, which meant
two rectangles per button, one drawn and one clicked, with nothing checking they agreed. With the
faces drawn there is no bleed and no second rectangle: `nf::HeaderGeometry::saveButton()` is both.

### Silence in, silence out — a DECLARED property

**TapeRot generates, deliberately, and is the one casting in the suite for which silence out would be
the bug.** The noise bed and hum are the plugin, not a leak.

**Measured 2026-08-14**, silence in, NOISE at 100 %:

| GEN | peak | rms |
|---|---|---|
| 1 | 0.035 (−29.1 dB) | 0.009 (−40.8 dB) |
| 4 | 0.107 (−19.4 dB) | 0.023 (−32.7 dB) |
| 8 | **0.375 (−8.5 dB)** | 0.078 (−22.2 dB) |

The rise with GEN is structural rather than incidental: from the second stage onward each
`DegradationCore` applies `gentleSaturationDrive = 1.35f`, so eight generations carry roughly 8× of
pre-saturation gain and the floor swells with the generation count. Eight generations of dubbing
sounding like eight generations of dubbing is the point of the plugin.

**The −8.5 dB figure at GEN 8 is recorded as a level question rather than a defect**, and it is the
chief's to answer against the plugin rather than against the number. It is here so that it is a
stated property nobody has to rediscover, and so the answer has something to be measured against.

Note the transport gate is `wrapperType != wrapperType_Standalone`: the generated noise is silenced
in a parked DAW session but not in the standalone, which has no transport for the gate to mean
anything about. Live monitoring through a *stopped* DAW is silenced too, which is a known open
question rather than a solved one — fixing it means gating the generated noise rather than the
finished buffer.

### Metering taps

The scope is fed by two read-only taps, both single-producer/single-consumer, lock-free, non-allocating, and both **drop rather than block** when full — an editor that is closed, stalled or repainting slowly costs the audio thread nothing.

- `FailureEngine`'s event FIFO (`pushEvent`/`popEvents`) drives the FAULT ACTIVITY dots. Its four `FailureEventType` values map 1:1 to DRP/SNG/CRK/WBL.
- `PitchDeviationMeter` carries the realised pitch deviation. `WowFlutter::process` takes an optional `float* deviationCentsAccum`; pitch deviation is the *rate of change* of the delay line, `cents = -1200/ln2 * d(delay)/dn`, measured on channel 0 only (channel 1 carries a phase offset that would partially cancel it) and accumulated across every active GEN stage. Decimation takes each window's **extreme**, not a point sample — flutter is spiky and point-sampling walks straight past a transient.

These measure the actual audio, never a knob. A wow/flutter/failure display trio was scaffolded once before and removed because it only mirrored its own knob's static value; `MeteringTests` asserts the difference (flutter must out-deviate wow at equal depth, which is only true of the derivative).

### Build system

`CMakeLists.txt` fetches JUCE via `FetchContent` (pinned to `8.0.14`) and defines one `juce_add_plugin(TapeRot ...)` target. `FORMATS` and `VST3_COPY_DIR`/`AU_COPY_DIR` are set from an `if(APPLE)`/`else()` block (`TAPEROT_FORMATS`/`TAPEROT_EXTRA_ARGS`) — AU only builds on Apple, and the Windows branch leaves the copy dirs unset so JUCE applies its own correct Windows default rather than a macOS path built on `$ENV{HOME}`. `CMAKE_OSX_DEPLOYMENT_TARGET`/`CMAKE_OSX_ARCHITECTURES` are likewise only set `if(APPLE)`. `PLUGIN_MANUFACTURER_CODE` (`Nfdy`, shared by every Neon Foundry plugin), `PLUGIN_CODE` (`Rota`), `BUNDLE_ID` (`com.neonfoundry.taperot`) and `COMPANY_NAME` (`Neon Foundry`) are settled, not placeholders — changing them again breaks saved projects in both AU and VST3, since JUCE derives the VST3 class ID from the manufacturer and plugin codes together. `Tests/` is a separate `juce_add_console_app` target that compiles the DSP `.cpp` files directly (not linked against the plugin target) plus its own Catch2-style test files — new DSP `.cpp` files need to be added to both `target_sources(TapeRot ...)` in the root `CMakeLists.txt` and `target_sources(TapeRotTests ...)` in `Tests/CMakeLists.txt` if you want them covered by tests.

`juce_add_binary_data(TapeRotBinaryData SOURCES ...)` embeds **three files** — the wordmark PNG,
Barlow Condensed SemiBold and Share Tech Mono — as C++ byte arrays. It was a deduplicated 2x sprite
set of 30 files until the panel rewrite rather than loading them from disk at runtime. Note JUCE's binary-data name-mangling strips non-alphanumerics rather than converting them to underscores, so `ShareTechMono-Regular.ttf` becomes `ShareTechMonoRegular_ttf`, not `ShareTechMono_Regular_ttf`. It's linked into both the `TapeRot` plugin target and `TapeRotTests` (anything that includes `TapeRotTheme.h` needs the link). New embedded assets go in this same `juce_add_binary_data` call, not as raw `target_sources`.

Unlike the icon, BinaryData **is** a build-time dependency and tracks correctly — changing an
embedded file and rebuilding picks it up without reconfiguring.

### Platform-specific notes

No exotic macOS APIs exist anywhere in `Source/DSP/` (confirmed by audit — no Objective-C++, no CoreAudio/CoreMIDI/AudioToolbox/Accelerate, no SIMD intrinsics), so the DSP side ports unmodified to both Windows and Linux. The only platform branches anywhere in the codebase are:

- the `CMakeLists.txt` items above (`if(APPLE)` gating `FORMATS`/copy dirs/deployment target).

`getUserProgramDirectory()` is `nf::userProgramDirectory` now — one implementation in
`neon-foundry-core`, consumed by every casting. User Programs live at
`<AppData>/<Manufacturer>/<Plugin>/Programs` on all three platforms.

**The "Application Support" segment is NOT JUCE's, and this file claimed it was.** JUCE resolves
`userApplicationDataDirectory` to `~/Library` on macOS — not `~/Library/Application Support` — while
it is `%APPDATA%` on Windows and `~/.config` on Linux, both already the right root. So macOS, and
only macOS, needs the segment appended by hand; JUCE's own `PropertiesFile` does it the same way,
guarded the same way. The paragraph here used to say the opposite, and all six castings wrote to
`~/Library/<Company>/` for a day because of it. It was caught by noticing a panel listing a Program
the filesystem did not have where the note said it would.

Two wrong claims in the same three lines, then, and they rhyme: "Presets is Apple's shared
convention directory, so our leaf belongs under it", and "JUCE resolves the segment for us". Both
sound like the kind of thing a good framework does, and both were settled in seconds by looking — at
what actually reads that folder, and at `juce_Files_mac.mm:210`.

The macOS branch used to point at `~/Library/Audio/Presets/<Manufacturer>/<Plugin>`, justified as
Apple's shared convention directory. **That reasoning was wrong**, and is worth recording so it
isn't reinstated: `~/Library/Audio/Presets` is where the **AU preset format** lives — `.aupreset`
files the AU system itself scans, reads and writes. A `.taperotprogram` sitting there was never
going to be discovered by anything, so the path bought no interoperability while asking Apple's
folder to hold a format it does not understand. All six castings now agree; Elmer had it right
first.

Everything else was already cross-platform-correct (icon generation, `createLegalFileName`, JUCE's own MSVC-aware recommended-flags targets) or has no Windows/Linux equivalent by nature (AU).

`.github/workflows/edge-build.yml` builds and publishes a macOS installer (`.pkg`, ad-hoc signed), a Windows installer (`installer/windows/TapeRot.iss`, built via Inno Setup - pre-installed on GitHub's Windows runners, no setup step needed - unsigned), and a Linux tarball (`.tar.gz` containing the VST3/Standalone plus `installer/linux/README.txt`, unsigned — no Linux equivalent of code-signing to bypass) to the same rolling `edge` GitHub pre-release on every push to `develop`. The Windows installer places the VST3 in `%COMMONPROGRAMFILES%\VST3\` and the Standalone under `{autopf}\TapeRot\` (Program Files), both requiring admin elevation. The Linux `build-linux` CI job runs on `ubuntu-22.04` (not `ubuntu-latest`) for an older glibc baseline and broader binary compatibility with users' distros, and needs the JUCE Linux apt dependency list (ALSA/JACK/FreeType/Fontconfig/X11/GTK/mesa dev packages — see the job or `BUILDING.md` for the full list) installed before configuring.

### The Program list's group caption

**Sized from its own type plus padding, never derived from the row height.** The construction is
`nf::captionHeight (font, topPadding, bottomPadding)` — 3px above and 4px below, the suite's adopted
default — and it comes out **19px** here, from Share Tech Mono at 11px through `withPointHeight`.

**The construction is the rule, not the number.** Writing 19 as a literal would break silently at
the first change of font, size or font construction, which is a change nobody would think to check a
caption against. It is also how this caption came to inherit JUCE's `rowHeight + rowHeight / 2` in
the first place — a caption half again *taller* than a row, which is a menu convention rather than a
panel one.

TapeRot and Fifth Member both land on 19, and Elmer reaches the same 19 from IBM Plex Mono at
9px — a coincidence of two line boxes (1.127 em against 1.300) meeting once the padding is added,
not a shared constant.
