#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The "TAPEROT" Dymo nameplate, blitted from a pre-baked transparent PNG.
//
// It used to be drawn live: a vector tape plate with hand-applied imperfections (rotation,
// asymmetric hand-cut edges, a slight bow, a contact shadow) carrying "TAPEROT" set in Impact Label,
// each character nudged by a deterministic per-character jitter. Every part of that was static and
// deterministic, so baking it is lossless.
//
// The reason to bake was platform divergence. Impact Label is a hand-drawn display face whose
// vertical-metrics tables the three text backends read very differently - the same requested height
// produced roughly 0.61x the cap-ink through CoreText and 1.00x through DirectWrite, so the label
// rendered about 1.6x taller on Windows than on macOS. That forced a pinned per-backend height
// constant in TapeRotTheme::dymoFont(), and the Linux value was never more than an unverified
// placeholder because FreeType is a third backend again. Baking on macOS - the reference the design
// was approved against - makes every platform render identical artwork and retires that whole class
// of discrepancy, along with the font from BinaryData.
//
// The PNG was produced by tools/BakeDymoLabel.cpp (a one-shot, since deleted), which called the
// previous live paint() offscreen at 3x. If it ever needs regenerating, recover that tool from this
// file's git history; design/impact-label/ still carries the font.
class DymoLabel final : public juce::Component
{
public:
    void paint(juce::Graphics&) override;
};
