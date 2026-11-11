#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "EditorFixtures.h"
#include "StubPlugin.h"

namespace
{
    struct LookAndFeelWithFonts : juce::LookAndFeel_V4, EditorFonts
    {
        juce::Font getEditorFont (float height, bool bold) override
        {
            asked.push_back ({ height, bold });
            return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
        }

        std::vector<std::pair<float, bool>> asked;
    };

    const HeaderBar::Layout layout { .height = 44, .edge = 12, .titleWidth = 128, .titleHeight = 18.0f,
                                     .gap = 12, .controlsHeight = 24, .meterWidth = 20, .meterInset = 4 };
}

//==============================================================================
TEST_CASE_METHOD (EditorFixtures::OverlayFixture, "Painted text takes its font from a LookAndFeel that gives one, and JUCE's default otherwise", "[editor][font]")
{
    juce::Component child;
    parent.addAndMakeVisible (child);

    const auto fallback = EditorFonts::of (child, 14.0f, true);
    CHECK_THAT (fallback.getHeight(), Catch::Matchers::WithinAbs (14.0, 1.0e-4));
    CHECK (fallback.isBold());
    CHECK (fallback.getTypefaceName() == juce::Font::getDefaultSansSerifFontName());

    LookAndFeelWithFonts lookAndFeel;
    parent.setLookAndFeel (&lookAndFeel);

    const auto given = EditorFonts::of (child, 12.0f, false);
    CHECK (given.getTypefaceName() == juce::Font::getDefaultMonospacedFontName());
    CHECK (lookAndFeel.asked == std::vector<std::pair<float, bool>> { { 12.0f, false } });

    parent.setLookAndFeel (nullptr);
}

TEST_CASE_METHOD (EditorFixtures::OverlayFixture, "The header's title, the manual's text and the prompt's text field use the LookAndFeel's font", "[editor][font]")
{
    LookAndFeelWithFonts lookAndFeel;
    parent.setLookAndFeel (&lookAndFeel);

    // Outlive the header that holds them
    struct Parts
    {
        StubPlugin::Processor processor;
        PresetManager presets { processor.getAPVTS(), {} };
        PromptOverlay prompt;
        juce::Component meter;
    } parts;

    {
        HeaderBar header { "TITLE", layout, parts.presets, parts.prompt, [] {}, "About", parts.meter };
        parent.addAndMakeVisible (header);

        CHECK (header.titleFont().getTypefaceName() == juce::Font::getDefaultMonospacedFontName());
        CHECK (lookAndFeel.asked.back() == std::pair { layout.titleHeight, true });
    }

    {
        lookAndFeel.asked.clear();
        ManualOverlay manual { "# Heading\n\nSome text.\n" };
        parent.addChildComponent (manual);
        manual.show();

        CHECK (lookAndFeel.asked == std::vector<std::pair<float, bool>> { { 16.0f, true }, { 12.0f, false } });
    }

    {
        lookAndFeel.asked.clear();
        PromptOverlay prompt;
        parent.addChildComponent (prompt);
        prompt.askForText ("Save", "Name", "Save", nullptr);

        const auto textFields = EditorFixtures::descendantsOf<juce::TextEditor> (prompt);
        REQUIRE (textFields.size() == 1);
        CHECK (textFields[0]->getFont().getTypefaceName() == juce::Font::getDefaultMonospacedFontName());
        CHECK (lookAndFeel.asked == std::vector<std::pair<float, bool>> { { 14.0f, false } });
    }

    parent.setLookAndFeel (nullptr);
}
