#include "Scope.h"
#include "TapeRotTheme.h"

namespace
{
    constexpr float scopeAmplitudePx = 40.0f;
    constexpr float carrierCycles = 2.2f;
}

Scope::Scope(TapeRotAudioProcessor& processor) : processorRef(processor), jitterRandom(90210)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
}

Scope::~Scope()
{
    stopTimer();
}

void Scope::timerCallback()
{
    repaint();
}

void Scope::paint(juce::Graphics& g)
{
    using namespace TapeRotTheme;

    std::array<float, TapeRotAudioProcessor::scopeHistorySize> levels{};
    int writeIndex = 0;
    processorRef.copyScopeLevels(levels, writeIndex);

    const float genValue = processorRef.getGenDisplay();
    const float dirtiness = juce::jlimit(0.0f, 1.0f, (genValue - 1.0f) / 7.0f);
    const float speed = juce::jlimit(0.0f, 1.0f, processorRef.getStopSpeedDisplay());

    const float width = Layout::scopeXEnd - Layout::scopeX;
    constexpr int numPoints = TapeRotAudioProcessor::scopeHistorySize;
    const float timeStretch = juce::jmax(0.15f, speed);
    const float ampScale = 0.35f + 0.65f * speed;

    juce::Path path;
    for (int i = 0; i < numPoints; ++i)
    {
        const int historyIndex = (writeIndex + i) % numPoints;
        const float level = levels[(size_t) historyIndex];

        const float t = (float) i / (float) (numPoints - 1);
        const float x = Layout::scopeX + t * width;

        const float carrier = std::sin(juce::MathConstants<float>::twoPi * carrierCycles * t / timeStretch);
        const float jitter = dirtiness * (jitterRandom.nextFloat() * 2.0f - 1.0f);
        const float amplitude = (0.15f + level * 0.85f) * ampScale;
        const float y = Layout::scopeCentreY - (carrier * amplitude + jitter * 0.3f) * scopeAmplitudePx;

        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(Colour::amber.withAlpha(0.3f));
    g.strokePath(path, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(Colour::amberBright);
    g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
