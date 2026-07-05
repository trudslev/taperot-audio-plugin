# TapeRot

A tape-degradation audio effect plugin (AU/VST3/Standalone) for macOS, built with JUCE 8.

TapeRot emulates the character of aging magnetic tape: drive/saturation, wow & flutter,
selectable tape-machine EQ models, noise/hum, a "failure" engine for dropouts/snags/crinkles,
stereo spread, and mix/output staging.

## Parameters

| Parameter | Range | Notes |
|---|---|---|
| Drive | 0-100% | Saturation amount |
| Wow | 0-100% | Slow pitch modulation |
| Flutter | 0-100% | Fast pitch modulation |
| Model | 8 choices | VCR HiFi, Camcorder, Dictaphone, Toy, Cassette Type I/II, Reel-to-Reel, Answering Machine |
| Noise | 0-100% | Tape hiss |
| Hum | on/off | Mains hum |
| Failure | 0-100% | Overall failure engine intensity |
| Dropouts / Snags / Crinkles / Imbalance | on/off | Individual failure modes |
| Spread | on/off | Stereo spread |
| Mix | 0-100% | Dry/wet |
| Output | -24..+24 dB | Output trim |

## Building

See [BUILDING.md](BUILDING.md) for build requirements, commands, validation (auval/pluginval),
and running the DSP test suite.

## Project layout

```
Source/
  PluginProcessor.*    Audio processor, parameter layout wiring
  PluginEditor.*        Editor shell, fixed-aspect-ratio scaling
  Parameters.h          APVTS parameter layout and IDs
  DSP/                  Saturator, WowFlutter, TapeModelEQ, NoiseGenerator,
                         FailureEngine, StereoSpread, OutputStage
  GUI/                  TapeRotTheme (layout/colour constants), TapeRotLookAndFeel,
                         SectionPanel, DymoLabel, TapeRotEditorContent
Tests/                   Catch2-style DSP unit tests (see BUILDING.md to run)
design/                  Reference SVG interface mock and app icon source/exports
```

## Status

Only blocks 1-3 of the signal chain (drive/saturation, wow & flutter, tape model EQ) have
real DSP implemented. Noise, the failure engine, stereo spread, and mix/output/bypass are
wired into the APVTS but currently passthrough stubs. See BUILDING.md for details.
