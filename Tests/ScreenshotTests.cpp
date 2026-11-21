#include <catch2/catch_test_macros.hpp>

#include <editor/editor.h>

namespace
{
    // Blue, with a red child one point square in the bottom right corner
    struct PaintedEditor : juce::Component
    {
        struct Corner : juce::Component
        {
            void paint (juce::Graphics& g) override   { g.fillAll (juce::Colours::red); }
        };

        PaintedEditor()
        {
            setOpaque (true);
            addAndMakeVisible (corner);
            setSize (440, 312);
        }

        void paint (juce::Graphics& g) override   { g.fillAll (juce::Colours::blue); }
        void resized() override                   { corner.setBounds (getLocalBounds().removeFromBottom (1).removeFromRight (1)); }

        Corner corner;
    };

    // As a web view: nothing JUCE paints, and a picture of its own
    struct SourceEditor : juce::Component, Screenshot::Source
    {
        void takeScreenshot (Screenshot::ImageCallback done) override   { done (picture); }

        juce::Image picture;
    };

    struct ScreenshotFixture
    {
        ScreenshotFixture()    { folder.createDirectory(); }
        ~ScreenshotFixture()   { folder.deleteRecursively(); }

        juce::Image pictureOf (juce::Component& editor)
        {
            juce::Image picture;
            Screenshot::of (editor, [&] (const juce::Image& taken) { picture = taken; });
            return picture;
        }

        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        juce::File folder { juce::File::createTempFile ("screenshots") };
    };
}

//==============================================================================
TEST_CASE_METHOD (ScreenshotFixture, "A screenshot is the editor's exact bounds at twice its size", "[screenshot][ui]")
{
    PaintedEditor editor;
    const auto picture = pictureOf (editor);

    REQUIRE (picture.getWidth() == 880);
    REQUIRE (picture.getHeight() == 624);

    CHECK (picture.getPixelAt (0, 0) == juce::Colours::blue);
    CHECK (picture.getPixelAt (877, 621) == juce::Colours::blue);

    // The last point of the editor is the last two pixels each way
    CHECK (picture.getPixelAt (878, 622) == juce::Colours::red);
    CHECK (picture.getPixelAt (879, 623) == juce::Colours::red);
}

TEST_CASE_METHOD (ScreenshotFixture, "An editor that takes its own screenshot isn't painted", "[screenshot][ui]")
{
    SourceEditor editor;
    editor.setSize (440, 312);
    editor.picture = juce::Image (juce::Image::RGB, 660, 468, true);

    const auto picture = pictureOf (editor);

    CHECK (picture.getWidth() == 660);
    CHECK (picture.getHeight() == 468);
}

TEST_CASE_METHOD (ScreenshotFixture, "A screenshot is saved as a PNG of the same size", "[screenshot][ui]")
{
    PaintedEditor editor;
    const auto file = folder.getChildFile ("Acme.png");

    REQUIRE (Screenshot::writePng (pictureOf (editor), file).wasOk());

    const auto saved = juce::PNGImageFormat::loadFrom (file);
    CHECK (saved.getWidth() == 880);
    CHECK (saved.getHeight() == 624);
    CHECK (saved.getPixelAt (879, 623) == juce::Colours::red);

    // Nothing left beside it
    CHECK (folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 1);
}

TEST_CASE_METHOD (ScreenshotFixture, "A screenshot that can't be taken or saved says so and leaves no file", "[screenshot][ui]")
{
    SECTION ("No picture")
    {
        SourceEditor editor;
        const auto file = folder.getChildFile ("Acme.png");

        const auto result = Screenshot::writePng (pictureOf (editor), file);

        CHECK (result.getErrorMessage() == "The screenshot couldn't be taken.");
        CHECK_FALSE (file.exists());
    }

    SECTION ("Nowhere to write")
    {
        const auto file = folder.getChildFile ("missing").getChildFile ("Acme.png");
        const auto result = Screenshot::writePng (juce::Image (juce::Image::RGB, 4, 4, true), file);

        CHECK (result.getErrorMessage() == "The screenshot couldn't be saved to " + file.getFullPathName());
        CHECK_FALSE (file.exists());
    }
}

TEST_CASE_METHOD (ScreenshotFixture, "A screenshot is named after the plugin and the time, and never replaces another", "[screenshot]")
{
    const juce::Time time (2026, 0, 31, 14, 3, 22);

    const auto first = Screenshot::fileIn (folder, "Acme", time);
    CHECK (first == folder.getChildFile ("Acme 2026-01-31 14.03.22.png"));

    REQUIRE (first.create().wasOk());
    CHECK (Screenshot::fileIn (folder, "Acme", time) == folder.getChildFile ("Acme 2026-01-31 14.03.22(2).png"));

    // A name a file can't have
    CHECK (Screenshot::fileIn (folder, "Acme: One", time).getFileName() == "Acme One 2026-01-31 14.03.22.png");
}
