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
    const char* name;
    std::array<EQBand, kMaxBandsPerModel> bands;
    int numBands;
};

inline constexpr std::array<TapeModel, 8> kTapeModels{{
    {"VCR HiFi", {{
        {EQBandType::LowShelf, 80.0f, 1.0f, 0.7f},
        {EQBandType::HighShelf, 12000.0f, -3.0f, 0.7f},
        {}
    }}, 2},
    {"Camcorder", {{
        {EQBandType::HighPass, 200.0f, 0.0f, 0.7f},
        {EQBandType::Peak, 1200.0f, 3.0f, 1.0f},
        {EQBandType::LowPass, 8000.0f, 0.0f, 0.7f}
    }}, 3},
    {"Dictaphone", {{
        {EQBandType::HighPass, 400.0f, 0.0f, 0.9f},
        {EQBandType::LowPass, 3500.0f, 0.0f, 0.9f},
        {EQBandType::Peak, 1000.0f, 4.0f, 1.5f}
    }}, 3},
    {"Toy", {{
        {EQBandType::HighPass, 200.0f, 0.0f, 0.8f},
        {EQBandType::Peak, 2500.0f, 8.0f, 4.0f},
        {EQBandType::LowPass, 2600.0f, 0.0f, 1.5f}
    }}, 3},
    {"Cassette Type I", {{
        {EQBandType::LowShelf, 60.0f, 2.0f, 0.7f},
        {EQBandType::HighShelf, 14000.0f, -4.0f, 0.7f},
        {}
    }}, 2},
    {"Cassette Type II", {{
        {EQBandType::LowShelf, 80.0f, 1.0f, 0.7f},
        {EQBandType::HighShelf, 16000.0f, -1.5f, 0.7f},
        {}
    }}, 2},
    {"Reel-to-Reel", {{
        {EQBandType::LowShelf, 60.0f, 0.5f, 0.7f},
        {EQBandType::HighShelf, 18000.0f, -1.0f, 0.7f},
        {}
    }}, 2},
    {"Answering Machine", {{
        {EQBandType::HighPass, 400.0f, 0.0f, 1.0f},
        {EQBandType::Peak, 1500.0f, 5.0f, 2.0f},
        {EQBandType::LowPass, 3000.0f, 0.0f, 1.0f}
    }}, 3},
}};

constexpr size_t kNumTapeModels = kTapeModels.size();
