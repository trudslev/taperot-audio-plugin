#pragma once

#include "WowFlutter.h"
#include "TapeModelEQ.h"
#include "NoiseSource.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

// One tape-generation "copy" worth of degradation: wow/flutter modulation, tape-model EQ, a small
// intrinsic saturation from the copy/dub process, and this generation's own contribution to the
// noise floor. GEN cascades several of these in series; each instance is independently seeded
// (via stageIndex) so multiple generations don't wobble in lockstep.
//
// Stage 0 (the first generation) skips the intrinsic saturation so that GEN=1 reproduces today's
// plain wow/flutter -> tape EQ -> noise pass exactly; from the second cascaded copy onward, each
// additional generation also introduces its own mild dub saturation.
class DegradationCore
{
public:
    explicit DegradationCore(int stageIndex);

    /** `initialModelIndex` is threaded through to TapeModelEQ — see its prepare for why it is an
        argument rather than something set afterwards. */
    void prepare(const juce::dsp::ProcessSpec& spec, int initialModelIndex,
                 float initialNoiseAmount01);
    void reset();
    // deviationCentsAccum is passed straight through to WowFlutter - see its comment. Null for any
    // stage whose modulation should not reach the scope.
    void process(juce::AudioBuffer<float>& buffer, float wow01, float flutter01, int model, bool clunkMode,
                 float noiseAmount01, int noiseCharacter, float* deviationCentsAccum = nullptr);

public:
    /*  **The per-stage wow RATE, drawn around the nominal rather than ramped away from it.**

        `WowFlutter::wowRateHz` is a `static constexpr 0.5f` shared by every stage, so the only thing
        that differed between stages was a seed and a 1.3 %-per-stage ramp — 9 % across all eight.
        Nine percent is not enough to decorrelate a 0.5 Hz oscillation over a musical passage, so the
        stages stayed phase-coherent and their deviations ADDED: measured **7.96x at GEN 8**, against
        8.00x for perfectly correlated sources.

        **Which is why per-stage SEEDING was not the fix, and was measured not to be.** Seeding sets
        the starting phase of eight oscillators running at the same rate; they beat, but they do not
        become independent. Independent transports differ in RATE — capstan diameter, motor
        regulation, belt wear — and rate difference is what makes the phases wander apart and keep
        wandering.

        Drawn rather than ramped for a reason this sweep has met before: a ramp makes the difference
        between adjacent stages identical, which is more regular than the mechanism permits. Real
        machines are not evenly spaced. The draw is deterministic in `stageIndex`, so the plugin
        stays reproducible. */
    static constexpr float wowRateSpread = 0.25f;   // +/- 25 % about the nominal rate

    static float wowRateMultiplierFor (int stageIndex) noexcept;

    /** The per-generation HF loss coefficient for a MODEL. Public so `GenerationCascadeTests` can
        rebuild the chain from the same figure rather than transcribing one — that test asserts the
        cascade equals its parts, so it has to rebuild the parts, and a transcribed constant would
        let the two drift apart while the test kept passing. Returns 0 for a model with no transfer
        loss, which is a pass-through rather than a filter at DC. */
    static float generationLossCoeffFor (double sampleRate, int modelIndex) noexcept;

private:
    static constexpr float gentleSaturationDrive = 1.35f;

    /*  **The per-generation HF loss, which is the law that SHOULD compound and did not exist.**

        A copy loses top end, and the loss multiplies. TapeRot measured 5 kHz moving **+0.8 dB**
        across eight generations — flat to slightly rising — so the plugin spent its whole generation
        budget on the two things that must not compound and none on the one that must. GEN sounded
        like more of everything rather than like generations.

        **The corner is PER MODEL** — `TapeModel::generationLossHz` — because generation loss is the
        dimension machines differ on most audibly, and one shared figure would make MODEL stop
        mattering at exactly the setting where it should matter most. */
    bool applySaturation;
    WowFlutter wowFlutter;
    std::vector<float> generationLossState;
    double generationLossSampleRate = 44100.0;

public:
    // Metering passthrough for the scope's rate readouts. Fixed per instance, so a plain read.
    float getWowRateHz() const noexcept { return wowFlutter.getWowRateHz(); }
    float getFlutterRateHz() const noexcept { return wowFlutter.getFlutterRateHz(); }

private:
    TapeModelEQ tapeModelEQ;
    NoiseSource noiseSource;
};
