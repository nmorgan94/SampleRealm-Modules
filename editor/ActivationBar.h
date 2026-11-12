#pragma once

#include "ActivationActions.h"
#include "PromptOverlay.h"

//==============================================================================
/**
 * The strip over the bottom of the editor until this machine is activated: its code, and the ways
 * to get a licence in. Mirrored by web/editor/ActivationBar.tsx.
 */
class ActivationBar : public juce::Component,
                      private juce::Timer
{
public:
    enum ColourIds
    {
        backgroundColourId = 0x5d10600,
        outlineColourId
    };

    static constexpr int height = 36;

    ActivationBar (licensing::Activation&, PromptOverlay&);

    // Installs the licence if it's valid here, or says why not
    void offer (const juce::String& licenceText)   { finish (actions.offer (licenceText)); }

    // Hides once activated, here or by another instance
    void update();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ActivationActions actions;
    PromptOverlay& prompt;

    juce::Label machineLabel;
    juce::TextButton activateButton { "Activate" }, pasteButton { "Paste" }, importButton { "Import" };

    // Shows what an action had to say, if anything
    void finish (const juce::String& message);

    void timerCallback() override   { if (isShowing()) update(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationBar)
};
