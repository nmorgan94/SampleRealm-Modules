#pragma once

// Mirrored by web/editor/about.ts
namespace About
{
    /** "VST3 in Ableton Live", or the format alone when the host is unknown. */
    juce::String running (const juce::AudioProcessor&);

    juce::String text (const juce::AudioProcessor&);
}
