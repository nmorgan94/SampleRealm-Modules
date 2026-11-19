#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "EditorFixtures.h"
#include "StubPlugin.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr const char* shapes[] { "Sine", "Saw", "Square" };

    constexpr ParameterSpecs::Spec level { "level", "Level", ParameterSpecs::Kind::real, -60.0f, 6.0f, -12.0f, 0.5f, 2.0f, "dB" };
    constexpr auto voices = ParameterSpecs::integer ("voices", "Voices", 1, 8, 4, "v");
    constexpr auto shape = ParameterSpecs::choice ("shape", "Shape", shapes, 1);
    constexpr auto mute = ParameterSpecs::toggle ("mute", "Mute", true);
    constexpr auto scope = ParameterSpecs::notAutomatable (ParameterSpecs::toggle ("scope", "Scope", false));
    constexpr std::array specs { &level, &voices, &shape, &mute, &scope };
}

//==============================================================================
TEST_CASE ("The layout has a parameter of the right kind for each spec, in order", "[parameters]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StubPlugin::SilentProcessor host;
    juce::AudioProcessorValueTreeState apvts { host, nullptr, "Parameters", createParameterLayout (specs, 3) };

    const auto& parameters = host.getParameters();
    REQUIRE (parameters.size() == 5);

    auto* levelParameter = dynamic_cast<juce::AudioParameterFloat*> (parameters[0]);
    REQUIRE (levelParameter != nullptr);
    CHECK (levelParameter->getParameterID() == "level");
    CHECK (levelParameter->getName (32) == "Level");
    CHECK (levelParameter->getLabel() == "dB");
    CHECK (levelParameter->getVersionHint() == 3);
    CHECK_THAT (levelParameter->range.start, WithinAbs (-60.0, 1.0e-6));
    CHECK_THAT (levelParameter->range.end, WithinAbs (6.0, 1.0e-6));
    CHECK_THAT (levelParameter->range.interval, WithinAbs (0.5, 1.0e-6));
    CHECK_THAT (levelParameter->range.skew, WithinAbs (2.0, 1.0e-6));
    CHECK_THAT (levelParameter->get(), WithinAbs (-12.0, 1.0e-6));

    auto* voicesParameter = dynamic_cast<juce::AudioParameterInt*> (parameters[1]);
    REQUIRE (voicesParameter != nullptr);
    CHECK (voicesParameter->getParameterID() == "voices");
    CHECK (voicesParameter->getLabel() == "v");
    CHECK (voicesParameter->getVersionHint() == 3);
    CHECK (voicesParameter->getRange() == juce::Range<int> (1, 8));
    CHECK (voicesParameter->get() == 4);

    auto* shapeParameter = dynamic_cast<juce::AudioParameterChoice*> (parameters[2]);
    REQUIRE (shapeParameter != nullptr);
    CHECK (shapeParameter->getParameterID() == "shape");
    CHECK (shapeParameter->getVersionHint() == 3);
    CHECK (shapeParameter->choices == juce::StringArray { "Sine", "Saw", "Square" });
    CHECK (shapeParameter->getCurrentChoiceName() == "Saw");

    auto* muteParameter = dynamic_cast<juce::AudioParameterBool*> (parameters[3]);
    REQUIRE (muteParameter != nullptr);
    CHECK (muteParameter->getParameterID() == "mute");
    CHECK (muteParameter->getName (32) == "Mute");
    CHECK (muteParameter->getVersionHint() == 3);
    CHECK (muteParameter->get());
    CHECK (muteParameter->isAutomatable());

    auto* scopeParameter = dynamic_cast<juce::AudioParameterBool*> (parameters[4]);
    REQUIRE (scopeParameter != nullptr);
    CHECK_FALSE (scopeParameter->get());
    CHECK_FALSE (scopeParameter->isAutomatable());

    CHECK (levelParameter->isAutomatable());
    CHECK (voicesParameter->isAutomatable());
    CHECK (shapeParameter->isAutomatable());
}

TEST_CASE ("A choice reads its names in any case, then numbers, from the host and from its knob", "[parameters]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StubPlugin::SilentProcessor host;
    juce::AudioProcessorValueTreeState apvts { host, nullptr, "Parameters", createParameterLayout (specs, 1) };
    auto& shapeParameter = *apvts.getParameter ("shape");

    CHECK_THAT (shapeParameter.getValueForText (" square "), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (shapeParameter.getValueForText ("SINE"), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (shapeParameter.getValueForText ("1"), WithinAbs (0.5, 1.0e-6));

    LabeledSlider control;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment { apvts, "shape", control.getSlider() };
    control.getSlider().textFromValueFunction = [] (double value) { return juce::StringArray { "SINE", "SAW", "SQUARE" }[juce::roundToInt (value)]; };
    auto& label = *EditorFixtures::descendantsOf<juce::Label> (control).front();

    // As if typed: the label's editor needs it on screen
    label.setText ("Sine", juce::sendNotificationSync);
    CHECK_THAT (shapeParameter.getValue(), WithinAbs (0.0, 1.0e-6));

    label.setText ("square", juce::sendNotificationSync);
    CHECK_THAT (shapeParameter.getValue(), WithinAbs (1.0, 1.0e-6));
    CHECK (label.getText() == "SQUARE");

    // Read as 0, which isn't its own name
    label.setText ("nothing", juce::sendNotificationSync);
    CHECK_THAT (shapeParameter.getValue(), WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("A real parameter can take value text of the plugin's own, and the rest keep theirs", "[parameters]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StubPlugin::SilentProcessor host;

    std::vector<juce::String> asked;
    const auto withLevelText = [&asked] (const ParameterSpecs::Spec& spec, juce::AudioParameterFloatAttributes attributes)
    {
        asked.push_back (spec.id);

        if (&spec != &level)
            return attributes;

        return attributes.withStringFromValueFunction ([] (float value, int) { return "at " + juce::String (juce::roundToInt (value)); })
                         .withValueFromStringFunction ([] (const juce::String& text) { return text.fromFirstOccurrenceOf ("at ", false, false).getFloatValue(); });
    };

    juce::AudioProcessorValueTreeState apvts { host, nullptr, "Parameters", createParameterLayout (specs, 1, withLevelText) };

    // Only real specs are asked about
    CHECK (asked == std::vector<juce::String> { "level" });

    auto& levelParameter = *apvts.getParameter ("level");
    CHECK (levelParameter.getText (levelParameter.convertTo0to1 (-12.0f), 100) == "at -12");
    CHECK_THAT (levelParameter.convertFrom0to1 (levelParameter.getValueForText ("at 3")), WithinAbs (3.0, 1.0e-4));
    CHECK (levelParameter.getLabel() == "dB");

    auto& voicesParameter = *apvts.getParameter ("voices");
    CHECK (voicesParameter.getText (voicesParameter.getValue(), 100) == "4");
}

TEST_CASE ("A real parameter's text has its step's decimal places, and no minus on a value that rounds to zero", "[parameters]")
{
    static constexpr ParameterSpecs::Spec offset { "offset", "Offset", ParameterSpecs::Kind::real, -0.25f, 0.25f, 0.0f };
    static constexpr std::array textSpecs { &level, &offset };

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StubPlugin::SilentProcessor host;
    juce::AudioProcessorValueTreeState apvts { host, nullptr, "Parameters", createParameterLayout (textSpecs, 1) };

    auto& levelParameter = *apvts.getParameter ("level");
    CHECK (levelParameter.getText (levelParameter.convertTo0to1 (-12.0f), 100) == "-12.0");
    CHECK (levelParameter.getText (levelParameter.convertTo0to1 (-12.0f), 3) == "-12");

    auto& offsetParameter = *apvts.getParameter ("offset");
    // Reads about -0.000000015
    const auto justBelowHalf = std::nextafter (0.5f, 0.0f);
    CHECK (offsetParameter.convertFrom0to1 (justBelowHalf) < 0.0f);
    CHECK (offsetParameter.getText (justBelowHalf, 100) == "0.0000000");
}

TEST_CASE ("A spec with a centre has that value half way along its range, as setSkewForCentre", "[parameters]")
{
    static constexpr auto tone = ParameterSpecs::withCentre ({ "tone", "Tone", ParameterSpecs::Kind::real, 200.0f, 16000.0f, 4000.0f }, 3000.0f);
    static constexpr std::array toneSpecs { &tone };

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StubPlugin::SilentProcessor host;
    juce::AudioProcessorValueTreeState apvts { host, nullptr, "Parameters", createParameterLayout (toneSpecs, 1) };

    juce::NormalisableRange<float> expected { 200.0f, 16000.0f };
    expected.setSkewForCentre (3000.0f);

    auto& toneParameter = *apvts.getParameter ("tone");
    CHECK (juce::exactlyEqual (toneParameter.getNormalisableRange().skew, expected.skew));
    CHECK_THAT (toneParameter.convertFrom0to1 (0.5f), WithinAbs (3000.0, 1.0e-2));

    // The web editor is told the same skew
    CHECK (juce::exactlyEqual (ParameterSpecs::skewOf (tone), expected.skew));
    CHECK (juce::exactlyEqual (ParameterSpecs::skewOf (level), 2.0f));
}
