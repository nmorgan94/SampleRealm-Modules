#pragma once

//==============================================================================
/** A rotary slider with its name below or above, which shows the value while it's being moved. Click
    the name to type a value. The value's text and the drag distance are the slider's own:
    getSlider().textFromValueFunction and setMouseDragSensitivity. Mirrored by web/editor/LabeledSlider.tsx. */
class LabeledSlider : public juce::Component,
                      private juce::Slider::Listener,
                      private juce::Timer
{
public:
    LabeledSlider()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.addListener (this);

        styleLabel (label);

        label.setEditable (true);
        label.onEditorShow = [this]
        {
            stopTimer(); // showing the name would close the editor
            if (auto* editor = label.getCurrentTextEditor())
            {
                editor->setText (valueText(), false);
                editor->selectAll();
            }
        };
        label.onEditorHide = [this] { startTimer (nameDelayMs); };
        // As Slider's text box, but snapped as setValue does and ignoring text that's neither a number nor a value's name
        label.onTextChange = [this]
        {
            const auto typed = label.getText().trim();
            const auto typedValue = slider.getNormalisableRange().snapToLegalValue (
                slider.snapValue (slider.getValueFromText (typed), juce::Slider::notDragging));

            // As JavaScript's parseFloat: it must start with a number
            const auto isNumber = typed.initialSectionContainingOnly ("+-.0123456789").containsAnyOf ("0123456789");
            const auto isName = slider.getTextFromValue (typedValue).equalsIgnoreCase (typed);

            if ((isNumber || isName) && ! juce::approximatelyEqual (typedValue, slider.getValue()))
            {
                juce::Slider::ScopedDragNotification drag (slider);
                slider.setValue (typedValue, juce::sendNotificationSync);
            }

            showValue();
        };

        addAndMakeVisible (slider);
        addAndMakeVisible (label);
    }

    /** Also the slider's title, which screen readers read. */
    void setLabelText (const juce::String& newText)
    {
        setName (newText);
        slider.setTitle (newText);
        showName();
    }

    juce::Slider& getSlider() noexcept { return slider; }

    enum class NamePlacement { below, above };

    /** Below, the name sits right under a square knob. Above, the knob takes all the room under the name. */
    void setNamePlacement (NamePlacement newPlacement)
    {
        placement = newPlacement;
        resized();
    }

    void setNameHeight (float fontHeight)
    {
        label.setFont (juce::FontOptions (fontHeight, juce::Font::bold));
    }

    // Other labelled controls can use these to match and line up with this one

    static constexpr int labelHeight = 24;
    static constexpr float nameHeight = 16.0f;

    static void styleLabel (juce::Label& labelBelow)
    {
        labelBelow.setFont (juce::FontOptions (nameHeight, juce::Font::bold));
        labelBelow.setJustificationType (juce::Justification::centred);
    }

    /** Puts the label right under a square control area, the pair centred, and returns that area. */
    static juce::Rectangle<int> layOutAboveLabel (juce::Label& labelBelow, juce::Rectangle<int> bounds)
    {
        const auto size = std::max (0, std::min (bounds.getWidth(), bounds.getHeight() - labelHeight));
        bounds = bounds.withSizeKeepingCentre (bounds.getWidth(), size + labelHeight);
        labelBelow.setBounds (bounds.removeFromBottom (labelHeight));
        return bounds.withSizeKeepingCentre (size, size);
    }

    void resized() override
    {
        if (placement == NamePlacement::below)
        {
            slider.setBounds (layOutAboveLabel (label, getLocalBounds()));
            return;
        }

        auto bounds = getLocalBounds();
        label.setBounds (bounds.removeFromTop (labelHeight));
        slider.setBounds (bounds);
    }

private:
    static constexpr int nameDelayMs = 600;

    juce::Slider slider;
    juce::Label label;
    NamePlacement placement = NamePlacement::below;
    bool dragging = false;

    void showName()             { label.setText (getName(), juce::dontSendNotification); }
    juce::String valueText()    { return slider.getTextFromValue (slider.getValue()); }
    void showValue()            { label.setText (valueText(), juce::dontSendNotification); }

    // The mouse wheel, double-click reset and typed values are drags too
    void sliderDragStarted (juce::Slider*) override     { stopTimer(); dragging = true; showValue(); }
    void sliderValueChanged (juce::Slider*) override    { if (dragging || isTimerRunning()) showValue(); }
    void sliderDragEnded (juce::Slider*) override       { dragging = false; startTimer (nameDelayMs); }

    void timerCallback() override
    {
        stopTimer();
        showName();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabeledSlider)
};
