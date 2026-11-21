#include "Screenshot.h"

namespace Screenshot
{
    // Pixels per point of an editor JUCE paints, whatever the display's
    constexpr float paintedScale = 2.0f;

    void of (juce::Component& editor, ImageCallback done)
    {
        if (auto* source = dynamic_cast<Source*> (&editor))
            source->takeScreenshot (std::move (done));
        else
            done (editor.createComponentSnapshot (editor.getLocalBounds(), true, paintedScale));
    }

    juce::Result writePng (const juce::Image& image, const juce::File& file)
    {
        if (! image.isValid())
            return juce::Result::fail ("The screenshot couldn't be taken.");

        const auto failed = juce::Result::fail ("The screenshot couldn't be saved to " + file.getFullPathName());

        // Moved into place once whole
        juce::TemporaryFile temporary (file);

        {
            juce::FileOutputStream stream (temporary.getFile());

            if (stream.failedToOpen() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
                return failed;

            // The PNG writer doesn't report a write that failed, as on a full disk
            stream.flush();

            if (stream.getStatus().failed())
                return failed;
        }

        return temporary.overwriteTargetFileWithTemporary() ? juce::Result::ok() : failed;
    }

    juce::File fileIn (const juce::File& folder, const juce::String& pluginName, juce::Time time)
    {
        const auto name = juce::File::createLegalFileName (pluginName + " " + time.formatted ("%Y-%m-%d %H.%M.%S"));
        return folder.getNonexistentChildFile (name, ".png");
    }

    void saveToDesktop (juce::AudioProcessorEditor& editor, std::function<void (const juce::Result&)> done)
    {
        of (editor, [pluginName = editor.processor.getName(), whenShown = std::move (done)] (const juce::Image& image)
        {
            // Named once the picture is here, so two taken in the same second don't share a name
            const auto file = fileIn (juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
                                      pluginName, juce::Time::getCurrentTime());
            const auto result = writePng (image, file);

            if (result.wasOk())
                file.revealToUser();

            whenShown (result);
        });
    }
}
