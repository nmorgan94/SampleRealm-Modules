#include <catch2/catch_test_macros.hpp>

#include <editor/editor.h>

#if JUCE_MODULE_AVAILABLE_licensing || JUCE_MODULE_AVAILABLE_activation
 #error "add_plugin_editor brought in licensing; a plugin without activation must have none"
#endif

#include "../StubPlugin.h"

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
