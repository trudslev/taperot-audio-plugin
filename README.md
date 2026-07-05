# TapeRot

A tape-degradation audio effect plugin (AU/VST3/Standalone) for macOS, built with JUCE 8.

TapeRot emulates the character of aging magnetic tape and the machines that play it: drive/saturation,
wow & flutter, 8 selectable tape-machine EQ models (with a generation-stacking cascade and a choice of
click-free crossfade or hard-clunk model switching), tape noise/hum, a "failure" engine for
dropouts/snags/crinkles/imbalance, global LP/HP tone shaping, stereo spread, momentary
STOP/FILTER/FAIL performance effects, and mix/output staging.

## Parameters

| Parameter | Range | Default | Notes |
|---|---|---|---|
| Drive | 0-100% | 20% | Saturation amount |
| Wow | 0-100% | 30% | Slow pitch modulation |
| Flutter | 0-100% | 25% | Fast pitch modulation |
| Model | 8 choices | Cassette I | REVOX B77, VCR HIFI, VCR LP, CAMCORDER, CASSETTE I, CASSETTE II, DICTAPHONE, TOY (cleanest to nastiest) |
| Switch | FADE / CLUNK | FADE | Model-switch behavior: click-free crossfade vs. hard swap + mute dip + mechanical thump |
| Generation (GEN) | 1-8 | 1 | Cascades the model EQ through N generations for compounding degradation |
| Noise | 0-100% | 0% | Tape hiss amount |
| Noise Character | TAPE / VCR / DUST | TAPE | Hiss colour/texture |
| Hum | on/off | off | Mains hum |
| LP / HP | 1-20kHz / 20-2000Hz | wide open (off) | Global tone-shaping filters |
| Failure | 0-100% | 0% | Overall failure engine intensity |
| Dropouts / Snags / Crinkles / Imbalance | on/off | on | Individual failure modes (gated by Failure amount) |
| Stop / Filter / Fail (aux) | momentary on/off | off | STOP (tape-stop pitch ramp-down), FILTER (sweep), FAIL (forced failure) performance triggers |
| Ramp | 0.05-4s | 0.3s | Shared ramp time for the Stop/Filter/Fail aux triggers |
| Spread | on/off | off | Stereo spread |
| Mix | 0-100% | 100% | Dry/wet |
| Output | -24..+24 dB | 0dB | Output trim |

## Building

See [BUILDING.md](BUILDING.md) for build requirements, commands, validation (auval/pluginval),
and running the DSP test suite.

## Project layout

```
Source/
  PluginProcessor.*    Audio processor: parameter caching, GEN-cascade orchestration, signal chain
  PluginEditor.*        Editor shell, fixed-aspect-ratio scaling
  Parameters.h          APVTS parameter layout, IDs, and legacy-session migration
  DSP/
    Saturator, WowFlutter          Drive and pitch-modulation stages
    TapeModelData, TapeModelEQ     The 8 tape-model EQ table and FADE/CLUNK switching engine
    DegradationCore                One GEN stage: WowFlutter -> TapeModelEQ -> NoiseSource
    NoiseSource, Hum                Tape hiss/VCR/dust noise, mains hum
    FailureEngine                   Dropouts/snags/crinkles/imbalance
    StereoSpread, ToneFilters       Stereo width, global LP/HP
    TapeStop, FilterSweep           STOP/FILTER momentary aux effects
    OutputStage                     Dry/wet mix and output trim
  GUI/
    TapeRotTheme                   Layout/colour constants (960x400 reference canvas)
    TapeRotLookAndFeel             Knob slider look-and-feel
    SectionPanel                   Static background/bezel/labels
    DymoLabel                      Hand-applied "TAPEROT" nameplate (embedded Impact Label font)
    ToggleSwitch, NoiseCharacterSwitch, FailureDotToggle, GenSelector,
    SmallKnob, AuxButton            Interactive controls bound to APVTS parameters
    Scope, FailLamp, GenDigitDisplay, ModelReadout   Live read-only indicators
    TapeRotEditorContent           Assembles and positions all of the above
Tests/                   Catch2-style DSP unit tests (see BUILDING.md to run)
design/                  Reference SVG interface mock, app icon source/exports, embedded label font
prompts/                 Numbered work-package prompts (gitignored, local-only)
```

## Status

All DSP stages have real processing implemented. The one intentional placeholder is the REVOX B77
tape model, whose EQ curve is deliberately near-transparent pending measured reference data (its
character mostly emerges through GEN cascading rather than its own EQ).
