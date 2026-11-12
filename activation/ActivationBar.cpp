#include "ActivationBar.h"

ActivationBar::ActivationBar (licensing::Activation& activationToShow, PromptOverlay& promptForMessages)
    : actions (activationToShow), prompt (promptForMessages)
{
    setTitle ("Activation");
    setVisible (actions.needsActivation());

    machineLabel.setText (actions.machineCode(), juce::dontSendNotification);
    machineLabel.setBorderSize ({});

    activateButton.onClick = [this] { actions.openBrowser(); };
    pasteButton.onClick    = [this] { finish (actions.paste()); };
    importButton.onClick   = [this] { actions.importFile ([this] (const juce::String& message) { finish (message); }); };

    for (auto* child : std::initializer_list<juce::Component*> { &machineLabel, &activateButton, &pasteButton, &importButton })
        addAndMakeVisible (child);

    if (isVisible())
        startTimer (1000);
}

void ActivationBar::finish (const juce::String& message)
{
    if (message.isNotEmpty())
        prompt.showMessage ("Activate", message);

    update();
}

void ActivationBar::update()
{
    actions.refresh();

    if (actions.needsActivation())
        return;

    stopTimer();
    setVisible (false);
}

//==============================================================================
void ActivationBar::paint (juce::Graphics& g)
{
    g.fillAll (findColour (backgroundColourId));
    g.setColour (findColour (outlineColourId));
    g.fillRect (getLocalBounds().removeFromTop (1));
}

void ActivationBar::resized()
{
    auto bounds = getLocalBounds().reduced (12, 6);

    for (auto* button : { &importButton, &pasteButton })
    {
        button->setBounds (bounds.removeFromRight (62));
        bounds.removeFromRight (6);
    }

    activateButton.setBounds (bounds.removeFromRight (84));
    bounds.removeFromRight (12);
    machineLabel.setBounds (bounds);
}
