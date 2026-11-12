#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <set>

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
TEST_CASE ("A two-weight title is squeezed to fit, then cut short with an ellipsis", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::Font bold (juce::FontOptions (18.0f, juce::Font::bold));
    const juce::Font regular (juce::FontOptions (18.0f, juce::Font::plain));
    const HeaderBar::Title title { "BOLD ", "REGULAR" };
    const auto width = juce::GlyphArrangement::getStringWidth (bold, title.bold) + juce::GlyphArrangement::getStringWidth (regular, title.regular);
    const juce::String ellipsis = juce::String::fromUTF8 ("\xe2\x80\xa6");

    const auto fits = HeaderBar::fitTitle (title, bold, regular, juce::roundToInt (std::ceil (width)));
    CHECK (fits.shown.bold == title.bold);
    CHECK (fits.shown.regular == title.regular);
    CHECK_THAT (fits.squeeze, Catch::Matchers::WithinAbs (1.0, 0.0));

    const auto squeezed = HeaderBar::fitTitle (title, bold, regular, juce::roundToInt (width * 0.8f));
    CHECK (squeezed.shown.regular == title.regular);
    CHECK (squeezed.squeeze < 1.0f);
    CHECK (squeezed.squeeze >= HeaderBar::minimumTitleScale);

    const auto cut = HeaderBar::fitTitle (title, bold, regular, juce::roundToInt (width * 0.5f));
    CHECK (cut.shown.bold == title.bold);
    CHECK (cut.shown.regular.endsWith (ellipsis));
    CHECK (cut.shown.regular.length() < title.regular.length());
    const auto cutWidth = juce::GlyphArrangement::getStringWidth (bold, cut.shown.bold) + juce::GlyphArrangement::getStringWidth (regular, cut.shown.regular);
    CHECK (cutWidth * cut.squeeze <= width * 0.5f + 1.0f);

    // Past the regular part, the bold part is cut too
    const auto boldCut = HeaderBar::fitTitle (title, bold, regular, 20);
    CHECK (boldCut.shown.bold.length() < title.bold.length());
    CHECK (boldCut.shown.regular == ellipsis);
}

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
        HeaderBar header { HeaderBar::Title { "BOLD ", "REGULAR" }, layout, parts.presets, parts.prompt, [] {}, "About", parts.meter };
        parent.addAndMakeVisible (header);
        header.setBounds (0, 0, 440, layout.height);

        CHECK (header.getTitle() == "BOLD REGULAR");

        const auto boldWidth = juce::GlyphArrangement::getStringWidth (header.titleFont(), "BOLD ");
        const auto regularWidth = juce::GlyphArrangement::getStringWidth (EditorFonts::of (header, layout.titleHeight, false), "REGULAR");
        CHECK_THAT (header.titleTextWidth(), Catch::Matchers::WithinAbs (boldWidth + regularWidth, 1.0e-3));

        lookAndFeel.asked.clear();
        header.createComponentSnapshot (header.getLocalBounds());
        // Both weights at the title's height, and nothing else
        const std::set<std::pair<float, bool>> fontsAsked (lookAndFeel.asked.begin(), lookAndFeel.asked.end());
        CHECK (fontsAsked == std::set<std::pair<float, bool>> { { layout.titleHeight, true }, { layout.titleHeight, false } });
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
