#include <catch2/catch_test_macros.hpp>

#include <editor/editor.h>

#if JUCE_MODULE_AVAILABLE_licensing || JUCE_MODULE_AVAILABLE_activation
 #error "add_plugin_editor brought in licensing; a plugin without activation must have none"
#endif

#include "../StubPlugin.h"
#include <samplerealm/BuildCommit.h>
#include <samplerealm/BuildType.h>

//==============================================================================
TEST_CASE ("The header and presets work in a plugin with no licensing", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    StubPlugin::Processor processor;
    PresetManager presets { processor.getAPVTS(), {} };
    PromptOverlay prompt;
    juce::Component meter;

    const HeaderBar::Layout layout { .height = 44, .edge = 12, .titleWidth = 128, .titleHeight = 18.0f,
                                     .gap = 12, .controlsHeight = 24, .meterWidth = 20, .meterInset = 4 };
    HeaderBar header { "TITLE", layout, presets, prompt, [] {}, "About", meter };

    CHECK (header.getTitle() == "TITLE");
    CHECK (presets.getCurrent().name == "Init");
}

TEST_CASE ("A plugin's own title takes the title's place in the header", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    StubPlugin::Processor processor;
    PresetManager presets { processor.getAPVTS(), {} };
    PromptOverlay prompt;
    juce::Component meter, ownTitle;
    ownTitle.setTitle ("Two lines");

    const HeaderBar::Layout layout { .height = 44, .edge = 12, .titleWidth = 128, .titleHeight = 18.0f,
                                     .gap = 12, .controlsHeight = 24, .meterWidth = 20, .meterInset = 4 };
    HeaderBar header { ownTitle, layout, presets, prompt, [] {}, "About", meter };
    header.setSize (440, layout.height);

    CHECK (header.getTitle() == "Two lines");
    CHECK (ownTitle.getParentComponent() == &header);
    CHECK (ownTitle.getBounds() == juce::Rectangle<int> (12, 0, 128, 44));
}

TEST_CASE ("A marked menu button has a dot in the title's colour at its top right corner", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    StubPlugin::Processor processor;
    PresetManager presets { processor.getAPVTS(), {} };
    PromptOverlay prompt;
    juce::Component meter;

    const HeaderBar::Layout layout { .height = 44, .edge = 12, .titleWidth = 128, .titleHeight = 18.0f,
                                     .gap = 12, .controlsHeight = 24, .meterWidth = 20, .meterInset = 4 };
    HeaderBar header { "TITLE", layout, presets, prompt, [] {}, "About", meter };
    header.setColour (HeaderBar::backgroundColourId, juce::Colours::black);
    header.setColour (HeaderBar::titleColourId, juce::Colours::red);
    header.setSize (440, layout.height);

    juce::TextButton* menuButton = nullptr;

    for (auto* child : header.getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr && button->getButtonText() == "...")
            menuButton = button;

    REQUIRE (menuButton != nullptr);

    const auto dotCentre = menuButton->getBounds().getTopRight().translated (-HeaderBar::menuMarkSize, HeaderBar::menuMarkSize);
    const auto colourAtDot = [&] { return header.createComponentSnapshot (header.getLocalBounds(), true, 1.0f).getPixelAt (dotCentre.x, dotCentre.y); };

    CHECK_FALSE (header.isMenuMarked());
    CHECK (colourAtDot() != juce::Colours::red);

    header.setMenuMarked (true);
    CHECK (colourAtDot() == juce::Colours::red);

    header.setMenuMarked (false);
    CHECK (colourAtDot() != juce::Colours::red);
}

TEST_CASE ("About shows the version and build type, the commit, where it runs, and the UI", "[editor]")
{
    StubPlugin::Processor processor;
    const auto lines = juce::StringArray::fromLines (About::text (processor));

    REQUIRE (lines.size() == 5);
    CHECK (lines[0] == "Version " JucePlugin_VersionString);
    CHECK (lines[1] == juce::String (BuildType::name) + " build");
    CHECK (lines[2] == juce::String ("Commit ") + buildCommit);
    CHECK (lines[3] == "Undefined");
    CHECK (lines[4] == "Native UI");
}
