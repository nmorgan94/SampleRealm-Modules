#pragma once

//==============================================================================
/** The page add_webview_ui builds into the plugin, with the functions and events it calls. */
class WebViewEditor final : public juce::AudioProcessorEditor,
                            public Screenshot::Source,
                            private juce::Timer
{
public:
    static constexpr int updateHz = LevelMeter::Ballistics::updateHz;

    /** Sends the page an event. Only for use inside the onTick call it's passed to. */
    using EmitEvent = std::function<void (const juce::Identifier& name, const juce::var& payload)>;

    /** The sizes an editor can be dragged between. */
    struct ResizeLimits
    {
        int minWidth, minHeight, maxWidth, maxHeight;
    };

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

       #if JUCE_MODULE_AVAILABLE_updates
        // Null in a plugin that doesn't ask the website for its latest version
        updates::UpdateCheck* updates = nullptr;
       #endif

        int width;
        int height;

        // Shown until the page draws
        juce::Colour background = juce::LookAndFeel_V4::getDarkColourScheme()
                                      .getUIColour (juce::LookAndFeel_V4::ColourScheme::UIColour::windowBackground);

        // Called tickHz times a second while the page is showing, for the plugin's own events
        std::function<void (const EmitEvent&)> onTick = nullptr;

        // Adds the plugin's own native functions and initialisation data
        std::function<juce::WebBrowserComponent::Options (juce::WebBrowserComponent::Options)> withOptions = nullptr;

        // A multiple of updateHz, for a plugin that draws faster than the meter. The level and the rest stay at updateHz
        int tickHz = updateHz;

        // For an editor the user can resize: width and height are then the size it opens at
        std::optional<ResizeLimits> resizeLimits = std::nullopt;

        // Called with the editor's size whenever it changes, for a plugin that keeps it
        std::function<void (int width, int height)> onResized = nullptr;
    };

    WebViewEditor (juce::AudioProcessor&, const Plugin&);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The page as the web view shows it, at the display's scale: JUCE can't paint it. */
    void takeScreenshot (Screenshot::ImageCallback done) override;

private:
    void timerCallback() override;

    juce::WebBrowserComponent::Options createOptions();
    juce::WebBrowserComponent::Options withPresetFunctions (const juce::WebBrowserComponent::Options&);
    juce::var activationStatus() const;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    Plugin plugin;

    // A tickHz that isn't a multiple of updateHz runs at the multiple below it
    const int ticksPerUpdate = juce::jmax (1, plugin.tickHz / updateHz);

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

   #if JUCE_MODULE_AVAILABLE_updates
    juce::WebBrowserComponent::Options withUpdateFunctions (const juce::WebBrowserComponent::Options&);
    juce::var updateStatus() const;

    std::unique_ptr<UpdateActions> updateActions;

    // Held while the page is hidden, which would miss it
    bool updateStatusUnsent = false;
   #endif

    juce::uint32 lastPresetChange = 0;
    int ticksSinceUpdate = 0;

    juce::MemoryInputStream uiZipStream;
    juce::ZipFile uiZip { uiZipStream };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewEditor)
};
