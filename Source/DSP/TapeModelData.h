#pragma once

#include <array>
#include <cstddef>

enum class EQBandType
{
    LowShelf,
    HighShelf,
    Peak,
    LowPass,
    HighPass
};

struct EQBand
{
    EQBandType type = EQBandType::Peak;
    float freqHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
};

constexpr int kMaxBandsPerModel = 3;

struct TapeModel
{
    const char* displayName;
    std::array<EQBand, kMaxBandsPerModel> bands;
    int numBands;
    // Per-model output trim so all loudness-match at matched settings. Calibrated by rendering
    // pink noise through each model and measuring RMS against the unprocessed input (see
    // Tests/TapeModelCalibrationTests.cpp) - each value exactly compensates that model's own
    // EQ-induced level change, so switching models changes tone, not volume.
    float makeupGainDb;

    /*  **The -3 dB corner of THIS machine's per-copy transfer loss, in Hz. 0 means none.**

        A first-order low-pass is -3.01 dB at its own cutoff, so this figure IS the "how many dB down
        at what frequency does one copy lose" spec rather than a fit to one. It compounds: eight
        passes through a 14 kHz corner are 24 dB down there.

        **Per model rather than one shared constant, and that is the point of the control.** MODEL is
        this plugin's signature, and generation loss is the dimension machines differ on most
        audibly — a shared figure would mean every machine loses the same top, so MODEL would stop
        mattering at exactly the setting where it should matter most. A B77 copy and a dictaphone
        copy are not the same copy.

        **Separate from `bands`, which is the machine's own tone for ONE pass.** The bands are
        calibrated and asserted against a single pass and against `makeupGainDb`; the transfer loss
        is what a COPY costs. Folding one into the other would make a single-pass tone change
        whenever a per-copy figure moved. */
    float generationLossHz;
};

// NONE first (index 0, the "least processing" option), then the eight real models ordered cleanest
// to nastiest so the rest of the MODEL control still sweeps hi-fi -> trash. NONE used to be
// appended last instead, specifically to keep the 8 real models' indices stable - moving it here
// intentionally breaks that stability, so see LegacyMigration's schema-version-3 remap in
// Parameters.h for how old sessions/presets are kept pointing at the right model.
inline constexpr std::array<TapeModel, 9> kTapeModels{{
    // No EQ, no makeup gain, no GEN cascading (PluginProcessor forces GEN to 1 generation when
    // this is selected) - a full bypass of the tape-model system for manual, unconstrained control
    // over DRIVE/WOW/FLUTTER/NOISE.
    {"NONE", {{ {}, {}, {} }}, 0, 0.0f, 0.0f},   // a full bypass, and GEN is forced to 1 here anyway
    // Approximates published Revox/Studer B77 specs at 7.5ips/19cm/s, cross-referenced across
    // multiple listings: frequency response 50Hz-15kHz +/-1.5dB (tight band), extending to
    // 30Hz-20kHz +2/-3dB (outer tolerance); S/N ~67dB(A) - the lowest noise floor of the eight
    // models; wow & flutter ~0.08% - the lightest of the eight; THD ~0.5% - the lowest drive
    // setting of the eight. No significant low-end head-bump (the real deck is close to flat
    // down low, unlike cassette-era gear). Still pending replacement with a real swept
    // measurement of an actual unit; this is a tolerance-band approximation, not a measured curve.
    {"REVOX B77", {{
        {EQBandType::HighPass, 28.0f, 0.0f, 0.707f},
        {EQBandType::HighShelf, 13000.0f, -2.0f, 0.707f},
        {}
    }}, 2, 1.26f, 18000.0f},   // open reel at 7.5 ips: the best transfer of the eight
    {"VCR HIFI", {{
        {EQBandType::HighShelf, 12000.0f, -2.0f, 0.707f},
        {EQBandType::Peak, 5000.0f, 1.0f, 1.0f},
        {}
    }}, 2, 0.01f, 15000.0f},   // FM carrier audio, wide for a consumer format
    {"VCR LP", {{
        {EQBandType::HighShelf, 8000.0f, -4.0f, 0.707f},
        {EQBandType::Peak, 1000.0f, -2.0f, 1.0f},
        {}
    }}, 2, 0.65f, 10000.0f},   // linear audio at long play, and its own bands already roll from 8k
    {"CAMCORDER", {{
        {EQBandType::HighPass, 200.0f, 0.0f, 0.707f},
        {EQBandType::LowPass, 8000.0f, 0.0f, 0.707f},
        {EQBandType::Peak, 1500.0f, 2.5f, 1.2f}
    }}, 3, 2.99f, 9000.0f},   // a worn portable, which is where the 9 kHz class sits
    // Revised from the prior "Cassette Type I" (LowShelf 60Hz +2dB/Q0.7, HighShelf 14kHz -4dB/Q0.7,
    // no makeup gain): the low end moved from a broad shelf to a narrower Peak bump (less mud below
    // 60Hz) and the treble rolloff was eased from -4dB to -2.5dB (brighter, less muffled) per this
    // model set's brief. The legacy state migration maps old "Cassette Type I" sessions onto this
    // model by name/position, but the curve itself is intentionally different, not a null-op.
    {"CASSETTE I", {{
        {EQBandType::HighShelf, 14000.0f, -2.5f, 0.707f},
        {EQBandType::Peak, 60.0f, 1.5f, 1.0f},
        {}
    }}, 2, -0.11f, 14000.0f},   // Type I in a good deck: -3 dB at 12-14 kHz per copy: -3 dB at 12-14 kHz per copy
    {"CASSETTE II", {{
        {EQBandType::HighShelf, 16000.0f, -1.0f, 0.707f},
        {EQBandType::LowShelf, 80.0f, -1.0f, 0.707f},
        {}
    }}, 2, 0.38f, 16000.0f},   // chrome holds its top better than Type I, which is what it is for
    {"DICTAPHONE", {{
        {EQBandType::HighPass, 400.0f, 0.0f, 0.8f},
        {EQBandType::LowPass, 3500.0f, 0.0f, 0.8f},
        {EQBandType::Peak, 3000.0f, 4.0f, 2.0f}
    }}, 3, 4.42f, 6000.0f},   // speech-band, and never intended to be copied
    {"TOY", {{
        {EQBandType::LowPass, 2500.0f, 0.0f, 1.2f},
        {EQBandType::HighPass, 300.0f, 0.0f, 0.8f},
        {}
    }}, 2, 4.22f, 4500.0f},   // the worst transfer in the set, deliberately
}};

constexpr size_t kNumTapeModels = kTapeModels.size();
constexpr size_t noneModelIndex = 0;
