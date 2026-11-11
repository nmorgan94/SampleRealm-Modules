#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "EditorFixtures.h"

using Catch::Matchers::WithinAbs;
using EditorFixtures::descendantsOf;

//==============================================================================
TEST_CASE ("A labelled slider shows its value while it's moved", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    LabeledSlider control;
    auto& slider = control.getSlider();
    slider.setRange (-60.0, 6.0, 0.1);
    slider.setTextValueSuffix (" dB");
    control.setLabelText ("GAIN");

    const auto labels = descendantsOf<juce::Label> (control);
    REQUIRE (labels.size() == 1);
    auto& label = *labels.front();
    CHECK (label.getText() == "GAIN");
    CHECK (slider.getTitle() == "GAIN");

    slider.setValue (-3.0, juce::sendNotificationSync);
    CHECK (label.getText() == "GAIN");

    {
        juce::Slider::ScopedDragNotification drag (slider);
        slider.setValue (-12.0, juce::sendNotificationSync);
        CHECK (label.getText() == "-12.0 dB");
    }

    // Until the timer brings the name back
    CHECK (label.getText() == "-12.0 dB");

    slider.setValue (-9.0, juce::sendNotificationSync);
    CHECK (label.getText() == "-9.0 dB");

    // As if typed: the label's editor needs it on screen
    label.setText ("-6", juce::sendNotificationSync);
    CHECK_THAT (slider.getValue(), WithinAbs (-6.0, 1.0e-9));
    CHECK (label.getText() == "-6.0 dB");

    struct GestureCounter : juce::Slider::Listener
    {
        int gestures = 0;
        void sliderValueChanged (juce::Slider*) override {}
        void sliderDragStarted (juce::Slider*) override   { ++gestures; }
    } counter;
    slider.addListener (&counter);

    // The same value, then text that doesn't start with a number
    label.setText ("GAIN", juce::dontSendNotification);
    label.setText ("-6.0 dB", juce::sendNotificationSync);
    label.setText ("abc", juce::sendNotificationSync);
    label.setText ("dB 5", juce::sendNotificationSync);
    CHECK (counter.gestures == 0);
    CHECK_THAT (slider.getValue(), WithinAbs (-6.0, 1.0e-9));
    CHECK (label.getText() == "-6.0 dB");

    slider.removeListener (&counter);
}

TEST_CASE ("A labelled control's label goes right under a square control, the pair centred", "[editor]")
{
    // The same cases as Tests/web/layout.test.ts
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::Label label;

    const auto check = [&label] (int width, int height, juce::Rectangle<int> control, juce::Rectangle<int> labelBounds)
    {
        CHECK (LabeledSlider::layOutAboveLabel (label, { width, height }) == control);
        CHECK (label.getBounds() == labelBounds);
    };

    check (140, 190, { 0, 13, 140, 140 }, { 0, 153, 140, 24 });
    check (100, 101, { 11, 0, 77, 77 }, { 0, 77, 100, 24 });
    check (50, 21, { 25, -1, 0, 0 }, { 0, -1, 50, 24 });
}
