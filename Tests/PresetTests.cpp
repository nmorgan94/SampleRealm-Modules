#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "EditorFixtures.h"
#include "PluginTestHelpers.h"
#include "StubPlugin.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

namespace
{
    juce::MemoryBlock factoryPreset (const juce::String& name, const juce::String& category, float gain)
    {
        const auto text = "<PRESET name=\"" + name + "\" category=\"" + category + "\" author=\"Example\" formatVersion=\"1\">"
                          "<Parameters><PARAM id=\"gain\" value=\"" + juce::String (gain) + "\"/></Parameters></PRESET>";

        return { text.toRawUTF8(), text.getNumBytesAsUTF8() };
    }

    // Factory presets given out of order, with user presets in a folder of their own
    struct PresetFixture : PreparedProcessor<StubPlugin::Processor>
    {
        ~PresetFixture()   { userDirectory.deleteRecursively(); }

        juce::File userDirectory = juce::File::createTempFile ("presets");

        PresetManager presets { processor.getAPVTS(), [this]
        {
            PresetManager::Config config;
            config.fileExtension = ".srtest";
            config.userDirectory = userDirectory;
            config.author = "Example";
            config.factoryPresets = { factoryPreset ("Cut -24", "Cut", -24.0f),
                                      factoryPreset ("Boost +6", "Boost", 6.0f),
                                      factoryPreset ("Cut -6", "Cut", -6.0f),
                                      factoryPreset ("Boost +3", "Boost", 3.0f),
                                      factoryPreset ("Cut -12", "Cut", -12.0f) };
            return config;
        }() };

        float gain()   { return getParameter (processor, StubPlugin::gainId); }
    };

    struct PresetOverlayFixture : EditorFixtures::OverlayFixture, PresetFixture {};

    juce::StringArray namesOf (const juce::Array<PresetManager::Preset>& list)
    {
        juce::StringArray names;

        for (const auto& preset : list)
            names.add (preset.name);

        return names;
    }

    const juce::StringArray factoryNames { "Boost +3", "Boost +6", "Cut -6", "Cut -12", "Cut -24" };
}

//==============================================================================
TEST_CASE_METHOD (PresetFixture, "Factory presets are listed by category, then name", "[presets]")
{
    CHECK (namesOf (presets.getFactoryPresets()) == factoryNames);
    CHECK (namesOf (presets.getPresets()) == factoryNames);

    for (const auto& preset : presets.getFactoryPresets())
    {
        CAPTURE (preset.name);
        CHECK (preset.isFactory);
        CHECK (preset.name.startsWith (preset.category));
    }
}

TEST_CASE_METHOD (PresetFixture, "A loaded preset sets its values and shows its name", "[presets]")
{
    CHECK (presets.getCurrent().name == "Init");

    REQUIRE (presets.loadPreset (presets.find ("Cut -12", true).value()));

    CHECK_THAT (gain(), WithinAbs (-12.0, 1.0e-4));
    CHECK (presets.getCurrent().name == "Cut -12");
    CHECK (presets.getCurrent().category == "Cut");
    CHECK (presets.getCurrent().isFactory);
    CHECK_FALSE (presets.isModified());
}

TEST_CASE_METHOD (PresetFixture, "Moving a parameter marks the preset modified until the next load", "[presets]")
{
    REQUIRE (presets.loadPreset (presets.find ("Boost +3", true).value()));

    setParameter (processor, StubPlugin::gainId, -1.0f);
    CHECK (presets.isModified());

    REQUIRE (presets.loadPreset (presets.find ("Boost +3", true).value()));
    CHECK_FALSE (presets.isModified());
    CHECK_THAT (gain(), WithinAbs (3.0, 1.0e-4));
}

TEST_CASE_METHOD (PresetFixture, "Changing state outside the parameters marks the preset modified", "[presets]")
{
    REQUIRE (presets.loadPreset (presets.find ("Boost +3", true).value()));

    processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails{}.withProgramChanged (true));
    CHECK_FALSE (presets.isModified());

    processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged (true));
    CHECK (presets.isModified());
}

TEST_CASE_METHOD (PresetFixture, "Init restores the defaults", "[presets]")
{
    REQUIRE (presets.loadPreset (presets.find ("Boost +6", true).value()));
    setParameter (processor, StubPlugin::gainId, 4.0f);

    presets.loadInit();

    CHECK_THAT (gain(), WithinAbs (StubPlugin::gain.defaultValue, 1.0e-4));
    CHECK (presets.getCurrent().name == "Init");
    CHECK_FALSE (presets.isModified());
}

TEST_CASE_METHOD (PresetFixture, "Next and previous wrap, and start from either end after Init", "[presets]")
{
    REQUIRE (presets.loadNext());
    CHECK (presets.getCurrent().name == factoryNames[0]);

    REQUIRE (presets.loadPrevious());
    CHECK (presets.getCurrent().name == factoryNames[factoryNames.size() - 1]);

    REQUIRE (presets.loadNext());
    CHECK (presets.getCurrent().name == factoryNames[0]);

    presets.loadInit();
    REQUIRE (presets.loadPrevious());
    CHECK (presets.getCurrent().name == factoryNames[factoryNames.size() - 1]);
    CHECK_THAT (gain(), WithinAbs (-24.0, 1.0e-4));
}

TEST_CASE_METHOD (PresetFixture, "User presets save, rename and delete in their folder", "[presets]")
{
    setParameter (processor, StubPlugin::gainId, -3.0f);

    REQUIRE (presets.saveUserPreset ("Mine", "Cut").wasOk());
    const auto mine = userDirectory.getChildFile ("Mine.srtest");
    CHECK (mine.existsAsFile());
    CHECK (presets.getCurrent().name == "Mine");
    CHECK_FALSE (presets.getCurrent().isFactory);
    CHECK_FALSE (presets.isModified());

    // Listed after the factory presets
    REQUIRE (presets.getPresets().getLast().file == mine);

    presets.loadInit();
    REQUIRE (presets.loadPreset (presets.getPresets().getLast()));
    CHECK_THAT (gain(), WithinAbs (-3.0, 1.0e-4));
    CHECK (presets.getCurrent().name == "Mine");

    REQUIRE (presets.renameUserPreset (presets.getPresets().getLast(), "Yours").wasOk());
    const auto yours = userDirectory.getChildFile ("Yours.srtest");
    CHECK_FALSE (mine.exists());
    CHECK (yours.existsAsFile());
    CHECK (presets.getCurrent().name == "Yours");
    CHECK (namesOf (presets.getUserPresets()) == juce::StringArray { "Yours" });

    REQUIRE (presets.deleteUserPreset (presets.getPresets().getLast()).wasOk());
    CHECK_FALSE (yours.exists());
    CHECK (presets.getUserPresets().isEmpty());
    CHECK (presets.isModified());
}

TEST_CASE_METHOD (PresetFixture, "User presets can't take a factory name", "[presets]")
{
    CHECK (presets.saveUserPreset ("boost +3", {}).failed());
    CHECK (presets.saveUserPreset ("   ", {}).failed());
    CHECK_FALSE (userDirectory.getChildFile ("boost +3.srtest").exists());

    REQUIRE (presets.saveUserPreset ("Mine", {}).wasOk());
    CHECK (presets.renameUserPreset (presets.getUserPresets().getFirst(), "Cut -6").failed());
    CHECK (userDirectory.getChildFile ("Mine.srtest").existsAsFile());
}

TEST_CASE_METHOD (PresetFixture, "Next and previous skip a user preset deleted outside the plugin", "[presets]")
{
    REQUIRE (presets.saveUserPreset ("A", {}).wasOk());
    REQUIRE (presets.saveUserPreset ("B", {}).wasOk());
    REQUIRE (presets.loadPreset (presets.find ("A", false).value()));

    REQUIRE (userDirectory.getChildFile ("B.srtest").deleteFile());

    REQUIRE (presets.loadNext());
    CHECK (presets.getCurrent().name == factoryNames[0]);
}

TEST_CASE_METHOD (PresetFixture, "Next and previous step past a preset that won't load", "[presets]")
{
    REQUIRE (presets.saveUserPreset ("A", {}).wasOk());
    REQUIRE (userDirectory.getChildFile ("Broken.srtest").replaceWithText ("not a preset"));

    REQUIRE (presets.loadNext());
    CHECK (presets.getCurrent().name == factoryNames[0]);

    REQUIRE (presets.loadPrevious());
    CHECK (presets.getCurrent().name == "A");
}

TEST_CASE_METHOD (PresetOverlayFixture, "Choosing a preset that won't load says so, and keeps the current one", "[presets][ui]")
{
    PromptOverlay prompt;
    parent.addChildComponent (prompt);
    PresetActions actions { presets, prompt };

    REQUIRE (presets.saveUserPreset ("A", {}).wasOk());
    REQUIRE (userDirectory.getChildFile ("Broken.srtest").replaceWithText ("not a preset"));
    presets.refreshUserPresets();

    actions.load (presets.find ("A", false).value());
    CHECK_FALSE (prompt.isVisible());

    actions.load (presets.find ("Broken", false).value());
    CHECK (prompt.isVisible());
    CHECK (presets.getCurrent().name == "A");
}

TEST_CASE_METHOD (PresetFixture, "Rescanning moves the change counter only when the folder changed", "[presets]")
{
    presets.refreshUserPresets();
    const auto before = presets.getChangeCounter();

    presets.refreshUserPresets();
    CHECK (presets.getChangeCounter() == before);

    REQUIRE (userDirectory.createDirectory().wasOk());
    REQUIRE (userDirectory.getChildFile ("Mine.srtest").replaceWithText ("<PRESET/>"));
    presets.refreshUserPresets();
    CHECK (presets.getChangeCounter() != before);
}

TEST_CASE_METHOD (PresetFixture, "State from before presets existed shows Init, modified", "[presets][state]")
{
    REQUIRE (presets.loadPreset (presets.find ("Boost +6", true).value()));

    juce::ValueTree state (processor.getAPVTS().state.getType());
    state.appendChild ({ "PARAM", { { "id", StubPlugin::gainId.getParamID() }, { "value", -20.0f } } }, nullptr);
    presets.restoreState (state);

    CHECK_THAT (gain(), WithinAbs (-20.0, 1.0e-4));
    CHECK (presets.getCurrent().name == "Init");
    CHECK (presets.isModified());
}

TEST_CASE_METHOD (PresetFixture, "The preset's name and modified flag travel with the saved state", "[presets][state]")
{
    REQUIRE (presets.loadPreset (presets.find ("Boost +6", true).value()));
    setParameter (processor, StubPlugin::gainId, 2.0f);

    auto saved = processor.getAPVTS().copyState();
    presets.addPresetInfoTo (saved);

    presets.loadInit();
    presets.restoreState (saved);

    CHECK (presets.getCurrent().name == "Boost +6");
    CHECK (presets.getCurrent().category == "Boost");
    CHECK (presets.getCurrent().isFactory);
    CHECK (presets.isModified());
    CHECK_THAT (gain(), WithinAbs (2.0, 1.0e-4));

    // Kept by the preset manager, not left on the parameters
    CHECK_FALSE (processor.getAPVTS().state.hasProperty ("presetName"));
}
