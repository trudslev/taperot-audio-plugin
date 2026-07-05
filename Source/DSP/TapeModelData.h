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
    // Placeholder curve pending measured reference data - deliberately subtle since a real B77
    // at 19cm/s is close to transparent; its character should mostly emerge via GEN compounding.
    {"REVOX B77", {{
        {EQBandType::Peak, 50.0f, 1.0f, 1.0f},
        {EQBandType::HighPass, 25.0f, 0.0f, 0.707f},
        {EQBandType::HighShelf, 18000.0f, -1.5f, 0.707f}
    }}, 3, 0.91f},
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
