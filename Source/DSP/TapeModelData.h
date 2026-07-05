#pragma once

#include <array>

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
    // Per-model output trim so all eight loudness-match at matched settings. Calibrated by
    // rendering pink noise through each model and measuring RMS against the unprocessed input
    // (see Tests/TapeModelCalibrationTests.cpp) - each value exactly compensates that model's own
    // EQ-induced level change, so switching models changes tone, not volume.
    float makeupGainDb;
};

// Ordered cleanest to nastiest so the MODEL knob sweeps hi-fi -> trash.
inline constexpr std::array<TapeModel, 8> kTapeModels{{
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
    }}, 2, 1.26f},
    {"VCR HIFI", {{
        {EQBandType::HighShelf, 12000.0f, -2.0f, 0.707f},
        {EQBandType::Peak, 5000.0f, 1.0f, 1.0f},
        {}
    }}, 2, 0.01f},
    {"VCR LP", {{
        {EQBandType::HighShelf, 8000.0f, -4.0f, 0.707f},
        {EQBandType::Peak, 1000.0f, -2.0f, 1.0f},
        {}
    }}, 2, 0.65f},
    {"CAMCORDER", {{
        {EQBandType::HighPass, 200.0f, 0.0f, 0.707f},
        {EQBandType::LowPass, 8000.0f, 0.0f, 0.707f},
        {EQBandType::Peak, 1500.0f, 2.5f, 1.2f}
    }}, 3, 2.99f},
    // Revised from the prior "Cassette Type I" (LowShelf 60Hz +2dB/Q0.7, HighShelf 14kHz -4dB/Q0.7,
    // no makeup gain): the low end moved from a broad shelf to a narrower Peak bump (less mud below
    // 60Hz) and the treble rolloff was eased from -4dB to -2.5dB (brighter, less muffled) per this
    // model set's brief. The legacy state migration maps old "Cassette Type I" sessions onto this
    // model by name/position, but the curve itself is intentionally different, not a null-op.
    {"CASSETTE I", {{
        {EQBandType::HighShelf, 14000.0f, -2.5f, 0.707f},
        {EQBandType::Peak, 60.0f, 1.5f, 1.0f},
        {}
    }}, 2, -0.11f},
    {"CASSETTE II", {{
        {EQBandType::HighShelf, 16000.0f, -1.0f, 0.707f},
        {EQBandType::LowShelf, 80.0f, -1.0f, 0.707f},
        {}
    }}, 2, 0.38f},
    {"DICTAPHONE", {{
        {EQBandType::HighPass, 400.0f, 0.0f, 0.8f},
        {EQBandType::LowPass, 3500.0f, 0.0f, 0.8f},
        {EQBandType::Peak, 3000.0f, 4.0f, 2.0f}
    }}, 3, 4.42f},
    {"TOY", {{
        {EQBandType::LowPass, 2500.0f, 0.0f, 1.2f},
        {EQBandType::HighPass, 300.0f, 0.0f, 0.8f},
        {}
    }}, 2, 4.22f},
}};

constexpr size_t kNumTapeModels = kTapeModels.size();
