#pragma once

#include "AuxEnvelope.h"
#include <juce_dsp/juce_dsp.h>

// Resonant low-pass sweep AUX effect: closes toward ~200Hz while held, reopens on release. Separate
// from ToneFilters' plain LP/HP (which are user tone controls, not a resonant sweep effect).
class FilterSweep
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, bool engaged, float rampSeconds);

private:
    static constexpr float openHz = 19500.0f;
    static constexpr float closedHz = 200.0f;
    static constexpr float resonance = 2.0f;

    juce::dsp::StateVariableTPTFilter<float> filter;
    AuxEnvelope envelope;
    double sampleRate = 44100.0;
};
