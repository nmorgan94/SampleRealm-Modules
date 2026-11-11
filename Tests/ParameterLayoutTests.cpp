#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "StubPlugin.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr const char* shapes[] { "Sine", "Saw", "Square" };

    constexpr ParameterSpecs::Spec level { "level", "Level", ParameterSpecs::Kind::real, -60.0f, 6.0f, -12.0f, 0.5f, 2.0f, "dB" };
    constexpr auto voices = ParameterSpecs::integer ("voices", "Voices", 1, 8, 4, "v");
    constexpr auto shape = ParameterSpecs::choice ("shape", "Shape", shapes, 1);
    constexpr std::array specs { &level, &voices, &shape };
}

//==============================================================================
TEST_CASE ("The layout has a parameter of the right kind for each spec, in order", "[parameters]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StubPlugin::SilentProcessor host;
    juce::AudioProcessorValueTreeState apvts { host, nullptr, "Parameters", createParameterLayout (specs, 3) };

    const auto& parameters = host.getParameters();
    REQUIRE (parameters.size() == 3);

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
}
