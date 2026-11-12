#pragma once

//==============================================================================
/**
 * What the activation bar's buttons do, for the native bar and the WebView's.
 * Each way of offering a licence gives a message for the user, empty when there is nothing to say.
 */
class ActivationActions
{
public:
    explicit ActivationActions (licensing::Activation&);

    // False once activated, and for a machine that gives no code, which can't be activated
    bool needsActivation() const;

    // The code as shown: K7Q2-M9XH-4TBV-8RNC
    juce::String machineCode() const;

    // Rereads the licence file, in case another instance activated this plugin
    void refresh()   { activation.refresh(); }

    void openBrowser();

    juce::String offer (const juce::String& licenceText);
    juce::String paste();

    // Called once the file is chosen, or with nothing to say if the chooser is cancelled
    void importFile (std::function<void (const juce::String& message)> whenDone);

private:
    licensing::Activation& activation;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool choosingFile = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationActions)
};
