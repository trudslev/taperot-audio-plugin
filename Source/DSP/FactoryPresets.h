#pragma once

#include "TapeModelData.h"
#include <array>

// A full parameter-state snapshot, one field per APVTS parameter except the three momentary aux
// triggers (STOP/FILTER/FAIL) - those are deliberately not part of a preset's own state at all;
// applying any preset always forces them false, so a preset can never load in a "stuck engaged"
// state (see PluginProcessor's applyProgramSnapshot).
struct FactoryPreset
{
    const char* name;
    const char* description;

    int modelIndex; // index into kTapeModels (see TapeModelData.h), or noneModelIndex
    float drivePercent;
    float wowPercent;
    float flutterPercent;
    float noisePercent;
    int noiseCharacter; // 0 = TAPE, 1 = VCR, 2 = DUST
    bool hum;
    float failurePercent;
    bool failureDropouts, failureSnags, failureCrinkles, failureImbalance;
    float mixPercent;
    float outputDb;
    bool spread;
    int gen; // 1-8
    float lpHz;
    float hpHz;
    float rampSeconds;
    bool switchMode; // false = FADE, true = CLUNK
};

// Ordered per the brief's own list. Values are directionally correct per each preset's
// description, not yet tuned by ear (that's a deliberate later pass - build, load, listen,
// adjust - not something to guess precisely up front).
inline constexpr std::array<FactoryPreset, 14> kFactoryPresets{{
    {"Init", "Clean pass-through starting point",
     (int) noneModelIndex, 0.0f, 0.0f, 0.0f, 0.0f, 0, false, 0.0f, true, true, true, true,
     100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false},

    {"Warm Cassette", "Just a little warmth",
     5 /* CASSETTE I */, 15.0f, 20.0f, 7.5f, 10.0f, 0, false, 0.0f, true, true, true, true,
     100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false},

    {"Revox Reference", "The cleanest model, barely-there tape warmth",
     1 /* REVOX B77 */, 8.0f, 8.0f, 3.0f, 5.0f, 0, false, 0.0f, true, true, true, true,
     100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false},

    {"VHS Memory", "Camcorder character, light occasional dropout",
     4 /* CAMCORDER */, 20.0f, 15.0f, 5.0f, 20.0f, 1 /* VCR */, false, 15.0f,
     true, false, false, false, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"Home Movie Night", "Camcorder pushed further: more failure, hum",
     4 /* CAMCORDER */, 25.0f, 20.0f, 7.5f, 45.0f, 1 /* VCR */, true, 40.0f,
     true, false, false, true, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"Answering Machine", "Dictaphone, narrowed band, occasional snag",
     7 /* DICTAPHONE */, 15.0f, 10.0f, 4.0f, 10.0f, 0 /* TAPE */, false, 30.0f,
     false, true, false, false, 100.0f, 0.0f, false, 1, 4000.0f, 300.0f, 0.3f, false},

    {"Bedroom 4-Track", "Cassette II, moderate drive and wow/flutter",
     6 /* CASSETTE II */, 35.0f, 30.0f, 12.5f, 25.0f, 0 /* TAPE */, false, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"Third Generation Dub", "Cassette I, GEN3, decorrelated stereo",
     5 /* CASSETTE I */, 20.0f, 25.0f, 10.0f, 20.0f, 0 /* TAPE */, false, 35.0f,
     true, true, true, true, 100.0f, 0.0f, true, 3, 20000.0f, 20.0f, 0.3f, false},

    {"Lo-Fi Beat Tape", "Toy model, dust noise, boxy LP-shaped tone",
     8 /* TOY */, 45.0f, 30.0f, 12.5f, 30.0f, 2 /* DUST */, false, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 2, 6000.0f, 20.0f, 0.3f, false},

    {"Radio Drift", "Toy model, prominent wow, thinned low end",
     8 /* TOY */, 25.0f, 55.0f, 10.0f, 15.0f, 0 /* TAPE */, true, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 1, 20000.0f, 250.0f, 0.3f, false},

    {"Fifth Gen Fade", "Cassette II, GEN5 - degraded but still musical",
     6 /* CASSETTE II */, 15.0f, 25.0f, 10.0f, 25.0f, 0 /* TAPE */, false, 15.0f,
     true, true, false, false, 100.0f, 0.0f, false, 5, 20000.0f, 20.0f, 0.3f, false},

    {"Dust and Crackle", "Revox B77, dust noise as the featured element",
     1 /* REVOX B77 */, 10.0f, 10.0f, 4.0f, 55.0f, 2 /* DUST */, false, 5.0f,
     true, false, false, false, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"Tape Stop Ready", "Near-clean base, short ramp for a snappy stop",
     5 /* CASSETTE I */, 10.0f, 10.0f, 4.0f, 5.0f, 0 /* TAPE */, false, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false},

    {"Total Meltdown", "As far as it goes - the one deliberate extreme",
     8 /* TOY */, 70.0f, 60.0f, 27.5f, 60.0f, 2 /* DUST */, true, 85.0f,
     true, true, true, true, 100.0f, 0.0f, true, 8, 20000.0f, 20.0f, 0.3f, false},
}};

constexpr size_t kNumFactoryPresets = kFactoryPresets.size();
constexpr size_t warmCassetteProgramIndex = 1;
