#pragma once

//==============================================================================
/**
 * A picture of an editor at exactly its bounds, for the header's menu.
 * Mirrored by web/editor/screenshotActions.ts.
 */
namespace Screenshot
{
    /** Given an invalid image when the picture couldn't be taken. May be called later. */
    using ImageCallback = std::function<void (const juce::Image&)>;

    /** An editor JUCE doesn't paint, as a web view, takes its own picture. */
    struct Source
    {
        virtual ~Source() = default;

        virtual void takeScreenshot (ImageCallback done) = 0;
    };

    void of (juce::Component& editor, ImageCallback done);

    juce::Result writePng (const juce::Image&, const juce::File&);

    /** "Acme 2026-01-31 14.03.22.png" in a folder, never a file already there. */
    juce::File fileIn (const juce::File& folder, const juce::String& pluginName, juce::Time);

    /** Saved on the desktop under the plugin's name, then shown in Finder or Explorer. */
    void saveToDesktop (juce::AudioProcessorEditor&, std::function<void (const juce::Result&)> done);
}
