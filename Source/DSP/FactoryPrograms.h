#pragma once

#include "TapeModelData.h"

#include <juce_core/juce_core.h>

#include <array>

/** Which list a Program belongs to. INIT is its own bank rather than a magic index, because it is
    in neither of the other two: the dropdown prints it unnumbered above FACTORY, the bank tag shows
    an em-dash rather than naming a bank it is not in, and DELETE is disabled on it. */
enum class ProgramBank
{
    init,
    factory,
    user,
    unresolved      ///< A stored identifier that no longer names anything. See ProgramId::id.
};

/** **How a Program is identified everywhere except the host adapter.**

    Not a position. Positions change when the bank is reordered or extended, and - while User
    Programs were on the host list - whenever the user saved or deleted one of their own. A stored
    position is a name that stops meaning the same thing.

    - `factory`    - `id` is the entry's permanent `FactoryProgram::slug`.
    - `user`       - `id` is the file's stem, which is also its displayed name.
    - `init`       - `id` is `"init"`.
    - `unresolved` - `id` is the identifier that failed to resolve, kept only so the panel can say
                     which Program the session meant. `displayName` is what actually gets painted;
                     see PluginProcessor::setStateInformation.

    `displayName` is carried alongside because a factory slug is not presentable - `warm-cassette?`
    in the LCD would read as a rendering fault rather than an unresolved Program. It is display only
    and never resolves anything. */
struct ProgramId
{
    ProgramBank bank = ProgramBank::factory;
    juce::String id;
    juce::String displayName;

    bool operator== (const ProgramId& other) const noexcept
    {
        return bank == other.bank && id == other.id;    // displayName is not identity
    }

    bool operator!= (const ProgramId& other) const noexcept { return ! operator== (other); }
};

// A full parameter-state snapshot, one field per APVTS parameter except the three momentary aux
// triggers (STOP/FILTER/FAIL) - those are deliberately not part of a program's own state at all;
// applying any program always forces them false, so a program can never load in a "stuck engaged"
// state (see PluginProcessor's applyProgramSnapshot).
struct FactoryProgram
{
    /** **The permanent identity, fixed at creation and never changed again.**

        A Program is identified by this, never by its position in the bank. `name` is a label the
        designers may revise; `slug` may not be revised, even if the name is, because it is what a
        saved session stores and what resolves back to this entry. Renaming "Warm Cassette" is free;
        renaming `warm-cassette` silently orphans every session that referenced it.

        Positions used to serve this purpose, and lifting Init out of the numbered bank showed what
        that costs: every index shifted by one, and without a hand-written remap every session would
        have reopened naming the Program AFTER the one it was saved with, values still correct. */
    const char* slug;

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

// Ordered per the brief's own list. Values are directionally correct per each program's
// description, not yet tuned by ear (that's a deliberate later pass - build, load, listen,
// adjust - not something to guess precisely up front).
inline constexpr std::array<FactoryProgram, 13> kFactoryPrograms{{
    {"warm-cassette", "Warm Cassette", "Just a little warmth",
     5 /* CASSETTE I */, 15.0f, 20.0f, 6.7f, 10.0f, 0, false, 0.0f, true, true, true, true,
     100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false},

    {"revox-reference", "Revox Reference", "The cleanest model, barely-there tape warmth",
     1 /* REVOX B77 */, 8.0f, 8.0f, 3.0f, 5.0f, 0, false, 0.0f, true, true, true, true,
     100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false},

    {"vhs-memory", "VHS Memory", "Camcorder character, light occasional dropout",
     4 /* CAMCORDER */, 20.0f, 15.0f, 5.0f, 20.0f, 1 /* VCR */, false, 15.0f,
     true, false, false, false, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"home-movie-night", "Home Movie Night", "Camcorder pushed further: more failure, hum",
     4 /* CAMCORDER */, 25.0f, 20.0f, 6.5f, 23.1f, 1 /* VCR */, true, 40.0f,
     true, false, false, true, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"answering-machine", "Answering Machine", "Dictaphone, narrowed band, occasional snag",
     7 /* DICTAPHONE */, 15.0f, 10.0f, 4.0f, 10.0f, 0 /* TAPE */, false, 30.0f,
     false, true, false, false, 100.0f, 0.0f, false, 1, 4000.0f, 300.0f, 0.3f, false},

    {"bedroom-4-track", "Bedroom 4-Track", "Cassette II, moderate drive and wow/flutter",
     6 /* CASSETTE II */, 35.0f, 30.0f, 9.2f, 13.4f, 0 /* TAPE */, false, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"third-generation-dub", "Third Generation Dub", "Cassette I, GEN3, decorrelated stereo",
     5 /* CASSETTE I */, 8.8f, 25.0f, 4.5f, 20.0f, 0 /* TAPE */, false, 35.0f,
     true, true, true, true, 100.0f, 0.0f, true, 3, 20000.0f, 20.0f, 0.3f, false},

    {"lo-fi-beat-tape", "Lo-Fi Beat Tape", "Toy model, dust noise, boxy LP-shaped tone",
     8 /* TOY */, 5.8f, 8.2f, 5.7f, 30.0f, 2 /* DUST */, false, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 2, 6000.0f, 20.0f, 0.3f, false},

    {"radio-drift", "Radio Drift", "Toy model, prominent wow, thinned low end",
     8 /* TOY */, 25.0f, 55.0f, 10.0f, 15.0f, 0 /* TAPE */, true, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 1, 20000.0f, 250.0f, 0.3f, false},

    {"fifth-gen-fade", "Fifth Gen Fade", "Cassette II, GEN5 - degraded but still musical",
     6 /* CASSETTE II */, 3.5f, 15.1f, 5.8f, 25.0f, 0 /* TAPE */, false, 15.0f,
     true, true, false, false, 100.0f, -5.1f, false, 5, 20000.0f, 20.0f, 0.3f, false},

    {"dust-and-crackle", "Dust and Crackle", "Revox B77, dust noise as the featured element",
     1 /* REVOX B77 */, 10.0f, 10.0f, 4.0f, 25.0f, 2 /* DUST */, false, 5.0f,
     true, false, false, false, 100.0f, 0.0f, false, 2, 20000.0f, 20.0f, 0.3f, false},

    {"tape-stop-ready", "Tape Stop Ready", "Near-clean base, short ramp for a snappy stop",
     5 /* CASSETTE I */, 10.0f, 10.0f, 4.0f, 5.0f, 0 /* TAPE */, false, 0.0f,
     true, true, true, true, 100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false},

    {"total-meltdown", "Total Meltdown", "As far as it goes - the one deliberate extreme",
     8 /* TOY */, 70.0f, 60.0f, 27.5f, 60.0f, 2 /* DUST */, true, 85.0f,
     true, true, true, true, 100.0f, 0.0f, true, 8, 20000.0f, 20.0f, 0.3f, false},
}};

constexpr size_t kNumFactoryPrograms = kFactoryPrograms.size();

/** **Warm Cassette is Program 01 and the instantiation default, and those are now the same thing.**
    It used to be index 1 behind Init, so the plugin opened on the second entry in its own list -
    which reads as a bug whether or not it is one. */
constexpr size_t warmCassetteProgramIndex = 0;

/** INIT's index. **-1, deliberately outside the bank rather than at position 0 within it.**

    INIT is not an authored sound competing with the other thirteen; it is the blank canvas you
    start from, so numbering it 01 would push RAIN ALL DAY to 02 and imply a running order it is not
    part of. Keeping it outside also means adding or removing it never renumbers anything.

    The cost is that -1 is now a meaningful index, so every "no index" sentinel in this casting has
    to be something else - see pendingProgramIndex, which moved to -2. */
constexpr int initProgramIndex = -1;

/** The blank canvas: the tape path present and audible in its plainest form, with everything that
    gives TapeRot its character at zero.

    Three rules decide every value, and they are not the same rule:
      - **Character and amount go to zero** - Drive, Wow, Flutter, Noise, Failure. Raise any one and
        you immediately hear what that one does.
      - **Structure goes to a usable middle, never zero** - Ramp at 0.3 s. A zero-length ramp is not
        neutral, it is a different (and broken) behaviour.
      - **Anything meaning "not acting" takes whatever value that is** - LP wide open at 20 kHz, HP
        at 20 Hz, Output 0 dB, Mix 100 %.

    **Mix is 100 %, not 50 %**, and that is the one value most likely to be "corrected" later.
    TapeRot is serial: the whole signal passes through the tape path. At 50 % the plugin would be
    half-bypassed rather than idle, and the first knob the user raised would appear weaker than it
    is. The four wet/dry castings sit at 50 % for the opposite reason.

    All four failure modes are left ON with Failure at 0 %: the amount is the control that silences
    them, and switching the modes off as well would mean the first thing a user did with the Failure
    knob produced nothing. */
inline constexpr FactoryProgram kInitProgram
    {"init", "INIT", "Clean pass-through starting point",
     (int) noneModelIndex, 0.0f, 0.0f, 0.0f, 0.0f, 0, false, 0.0f, true, true, true, true,
     100.0f, 0.0f, false, 1, 20000.0f, 20.0f, 0.3f, false};
