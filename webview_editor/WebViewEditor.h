#pragma once

//==============================================================================
/** The page add_webview_ui builds into the plugin, with the functions and events it calls. */
class WebViewEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    static constexpr int updateHz = LevelMeter::Ballistics::updateHz;

    /** Sends the page an event. Only for use inside the onTick call it's passed to. */
    using EmitEvent = std::function<void (const juce::Identifier& name, const juce::var& payload)>;

    struct Plugin
    {
        std::span<const ParameterSpecs::Spec* const> specs;
        juce::AudioProcessorValueTreeState& parameters;

        // Null for a plugin without the header's presets or meter: its page uses editorBackend()
        PresetManager* presets = nullptr;
        PeakMeter* meter = nullptr;

       #if JUCE_MODULE_AVAILABLE_activation
        // Null in a build that doesn't require activation
        licensing::Activation* activation = nullptr;
       #endif

        int width;
        int height;

        // Shown until the page draws
        juce::Colour background = juce::LookAndFeel_V4::getDarkColourScheme()
                                      .getUIColour (juce::LookAndFeel_V4::ColourScheme::UIColour::windowBackground);

        // Called updateHz times a second while the page is showing, for the plugin's own events
        std::function<void (const EmitEvent&)> onTick = nullptr;
    };

    WebViewEditor (juce::AudioProcessor&, const Plugin&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::WebBrowserComponent::Options createOptions();
    juce::WebBrowserComponent::Options withPresetFunctions (const juce::WebBrowserComponent::Options&);
    juce::var activationStatus() const;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    Plugin plugin;

   #if JUCE_MODULE_AVAILABLE_activation
    juce::WebBrowserComponent::Options withActivationFunctions (const juce::WebBrowserComponent::Options&);
    void checkActivation();

    std::unique_ptr<ActivationActions> activation;
    bool activationBarShowing = false;
    int ticksSinceActivationCheck = 0;
   #endif

    // Declaration order matters: relays, browser, attachments
    std::vector<std::unique_ptr<juce::WebSliderRelay>> relays;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> attachments;

    juce::uint32 lastPresetChange = 0;

    juce::MemoryInputStream uiZipStream;
    juce::ZipFile uiZip { uiZipStream };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewEditor)
};
