#include "ActivationActions.h"

// What to tell the user about a licence they offered, nothing once it is installed
static juce::String messageFor (licensing::LicenceStatus offered)
{
    using licensing::LicenceStatus;

    switch (offered)
    {
        case LicenceStatus::valid:           return {};
        case LicenceStatus::malformed:       return "That isn't a licence. Copy it from the website again.";
        case LicenceStatus::signature:
        case LicenceStatus::payload:         return "That licence isn't one this plugin accepts.";
        case LicenceStatus::wrongMachine:    return "That licence is for another computer. Activate again from this one.";
        case LicenceStatus::noLicenceFile:   return "The licence couldn't be saved.";
        case LicenceStatus::noMachineCode:   return "This computer gave no ID to activate with.";
    }

    return {};
}

ActivationActions::ActivationActions (licensing::Activation& activationToChange)
    : activation (activationToChange)
{
}

bool ActivationActions::needsActivation() const
{
    return activation.machine().has_value() && ! activation.isActivated();
}

juce::String ActivationActions::machineCode() const
{
    const auto& machine = activation.machine();
    return machine ? machine->grouped() : juce::String();
}

void ActivationActions::openBrowser()
{
    juce::URL (activation.beginInBrowser()).launchInDefaultBrowser();
}

juce::String ActivationActions::offer (const juce::String& licenceText)
{
    return messageFor (activation.install (licenceText.toStdString()));
}

juce::String ActivationActions::paste()
{
    const auto copied = juce::SystemClipboard::getTextFromClipboard();
    return copied.trim().isEmpty() ? juce::String ("Copy the licence from the website first.") : offer (copied);
}

void ActivationActions::importFile (std::function<void (const juce::String& message)> whenDone)
{
    // Replacing an open chooser would drop its callback
    if (choosingFile)
    {
        whenDone ({});
        return;
    }

    choosingFile = true;
    fileChooser = std::make_unique<juce::FileChooser> ("Choose your licence file", juce::File(), "*.licence");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, report = std::move (whenDone)] (const juce::FileChooser& chooser)
    {
        choosingFile = false;
        const auto chosen = chooser.getResult();

        if (chosen == juce::File())
            report ({});
        else
            report (offer (std::cmp_greater (chosen.getSize(), licensing::largestLicenceBytes) ? juce::String() : chosen.loadFileAsString()));
    });
}
