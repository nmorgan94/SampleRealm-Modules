#pragma once

//==============================================================================
/** The page add_webview_ui builds into the plugin, with the functions and events it calls. */
class WebViewEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    struct Plugin
    {
        std::span<const ParameterSpecs::Spec* const> specs;
        juce::AudioProcessorValueTreeState& parameters;
        PresetManager& presets;
        PeakMeter& meter;

        // Null in a build that doesn't require activation
        licensing::Activation* activation = nullptr;

        int width;
        int height;
    };

    WebViewEditor (juce::AudioProcessor&, const Plugin&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::WebBrowserComponent::Options createOptions();
    juce::WebBrowserComponent::Options withPresetFunctions (const juce::WebBrowserComponent::Options&);
    juce::WebBrowserComponent::Options withActivationFunctions (const juce::WebBrowserComponent::Options&);
    juce::var activationStatus() const;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    Plugin plugin;

    std::unique_ptr<ActivationActions> activation;
    bool activationBarShowing = false;
    int ticksSinceActivationCheck = 0;

    // Declaration order matters: relays, browser, attachments
    std::vector<std::unique_ptr<juce::WebSliderRelay>> relays;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> attachments;

    juce::uint32 lastPresetChange = 0;

    juce::MemoryInputStream uiZipStream;
    juce::ZipFile uiZip { uiZipStream };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewEditor)
};
