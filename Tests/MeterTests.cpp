#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "EditorFixtures.h"

using Catch::Matchers::WithinAbs;
using Ballistics = LevelMeter::Ballistics;

//==============================================================================
TEST_CASE ("Level meter bars rise at once and fall smoothly", "[meter][ui]")
{
    Ballistics bar;

    CHECK_THAT (bar.levelDb, WithinAbs (Ballistics::floorDb, 0.0));

    CHECK (bar.update (0.5f));
    CHECK_THAT (bar.levelDb, WithinAbs (-6.02, 0.01));

    CHECK (bar.update (1.0f));
    CHECK_THAT (bar.levelDb, WithinAbs (0.0, 1.0e-6));

    // One second of silence falls 20 dB
    for (int tick = 0; tick < Ballistics::updateHz; ++tick)
        bar.update (0.0f);

    CHECK_THAT (bar.levelDb, WithinAbs (-20.0, 0.01));

    // Then it settles on the floor and stops asking for repaints
    for (int tick = 0; tick < 10 * Ballistics::updateHz; ++tick)
        bar.update (0.0f);

    CHECK_THAT (bar.levelDb, WithinAbs (Ballistics::floorDb, 0.0));
    CHECK_FALSE (bar.update (0.0f));
}

TEST_CASE ("Level meter clip light holds after going over 0 dB", "[meter][ui]")
{
    Ballistics bar;

    const auto peak = GENERATE (1.5f, std::numeric_limits<float>::infinity());
    CAPTURE (peak);

    CHECK_FALSE (bar.isClipping());
    bar.update (peak);

    CHECK (bar.isClipping());
    CHECK_THAT (bar.levelDb, WithinAbs (0.0, 0.0));    // capped at the top of the scale

    for (int tick = 1; tick < Ballistics::clipHoldTicks; ++tick)
        bar.update (0.0f);

    CHECK (bar.isClipping());
    CHECK (bar.levelDb < 0.0f);     // even an infinite peak falls away

    CHECK (bar.update (0.0f));      // the light going out needs a repaint
    CHECK_FALSE (bar.isClipping());
}

TEST_CASE ("Level meter paints one centred bar per channel in the look and feel's colours", "[meter][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto meterChannels = GENERATE (1, 2);
    CAPTURE (meterChannels);

    PeakMeter source { meterChannels };
    LevelMeter meter { source };
    meter.setSize (LevelMeter::preferredWidth, 120);

    juce::Colour expected;

    SECTION ("With no colours set, it uses the LookAndFeel_V4 slider background")
    {
        expected = juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::Slider::backgroundColourId);
    }

    SECTION ("A colour set on the meter overrides that")
    {
        expected = juce::Colours::red;
        meter.setColour (LevelMeter::backgroundColourId, expected);
    }

    // Bars start empty, so each one's middle shows the background colour
    const auto image = meter.createComponentSnapshot (meter.getLocalBounds());
    const auto middle = LevelMeter::preferredWidth / 2;
    const auto leftBar = LevelMeter::barWidth / 2;
    const auto rightBar = LevelMeter::preferredWidth - LevelMeter::barWidth / 2 - 1;

    if (meterChannels == 1)
    {
        CHECK (image.getPixelAt (middle, 60) == expected);
        CHECK (image.getPixelAt (leftBar, 60).isTransparent());
    }
    else
    {
        CHECK (image.getPixelAt (leftBar, 60) == expected);
        CHECK (image.getPixelAt (rightBar, 60) == expected);
        CHECK (image.getPixelAt (middle, 60).isTransparent());
    }
}
