#pragma once

#include "WowFlutter.h"
#include "TapeModelEQ.h"
#include "NoiseSource.h"
#include <juce_dsp/juce_dsp.h>

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

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    // deviationCentsAccum is passed straight through to WowFlutter - see its comment. Null for any
    // stage whose modulation should not reach the scope.
    void process(juce::AudioBuffer<float>& buffer, float wow01, float flutter01, int model, bool clunkMode,
                 float noiseAmount01, int noiseCharacter, float* deviationCentsAccum = nullptr);

private:
    static constexpr float wowFlutterDetunePerStage = 0.013f;
    static constexpr float gentleSaturationDrive = 1.35f;

    bool applySaturation;
    WowFlutter wowFlutter;

public:
    // Metering passthrough for the scope's rate readouts. Fixed per instance, so a plain read.
    float getWowRateHz() const noexcept { return wowFlutter.getWowRateHz(); }
    float getFlutterRateHz() const noexcept { return wowFlutter.getFlutterRateHz(); }

private:
    TapeModelEQ tapeModelEQ;
    NoiseSource noiseSource;
};
