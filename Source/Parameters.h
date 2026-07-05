#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    constexpr auto drive = "drive";
    constexpr auto wow = "wow";
    constexpr auto flutter = "flutter";
    constexpr auto model = "model";
    constexpr auto noise = "noise";
    constexpr auto hum = "hum";
    constexpr auto failure = "failure";
    constexpr auto mix = "mix";
    constexpr auto output = "output";
    constexpr auto spread = "spread";
    constexpr auto failureDropouts = "failureDropouts";
    constexpr auto failureSnags = "failureSnags";
    constexpr auto failureCrinkles = "failureCrinkles";
    constexpr auto failureImbalance = "failureImbalance";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createTapeRotParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto percentAttrs = juce::AudioParameterFloatAttributes().withLabel("%");
    auto dbAttrs = juce::AudioParameterFloatAttributes().withLabel("dB");

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::drive, 1}, "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f), 20.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::wow, 1}, "Wow",
        juce::NormalisableRange<float>(0.0f, 100.0f), 30.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::flutter, 1}, "Flutter",
        juce::NormalisableRange<float>(0.0f, 100.0f), 25.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::model, 1}, "Model",
        juce::StringArray{"VCR HiFi", "Camcorder", "Dictaphone", "Toy",
                           "Cassette Type I", "Cassette Type II",
                           "Reel-to-Reel", "Answering Machine"},
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::noise, 1}, "Noise",
        juce::NormalisableRange<float>(0.0f, 100.0f, 50.0f), 0.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::hum, 1}, "Hum", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::failure, 1}, "Failure",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::mix, 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f, percentAttrs));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::output, 1}, "Output",
        juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f, dbAttrs));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::spread, 1}, "Spread", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureDropouts, 1}, "Dropouts", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureSnags, 1}, "Snags", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureCrinkles, 1}, "Crinkles", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::failureImbalance, 1}, "Imbalance", true));

    return {params.begin(), params.end()};
}
