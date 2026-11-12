// The page side of these names is web/plugin/pluginBackend.ts
#include <samplerealm/BuildCommit.h>
#include <samplerealm/UiInfoJson.h>
#include <WebUIData.h>

#if JUCE_MAC
 #import <Foundation/Foundation.h>
#endif

namespace
{
    const auto background = juce::LookAndFeel_V4::getDarkColourScheme()
                                .getUIColour (juce::LookAndFeel_V4::ColourScheme::UIColour::windowBackground);

    const char* mimeTypeFor (const juce::String& fileName)
    {
        static const std::unordered_map<juce::String, const char*> types {
            { "html", "text/html" },
            { "js",   "text/javascript" },
            { "css",  "text/css" },
            { "ttf",  "font/ttf" },
            { "woff2", "font/woff2" },
            { "svg",  "image/svg+xml" },
            { "png",  "image/png" },
            { "json", "application/json" }
        };

        const auto it = types.find (fileName.fromLastOccurrenceOf (".", false, false).toLowerCase());
        return it != types.end() ? it->second : "application/octet-stream";
    }

    std::vector<std::byte> readAll (juce::InputStream& stream)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (stream.getTotalLength()));
        stream.read (bytes.data(), static_cast<int> (bytes.size()));
        return bytes;
    }

    struct SinglePageBrowser final : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL == getResourceProviderRoot();
        }
    };

    juce::var toVar (const juce::String& name, const juce::String& category, bool isFactory)
    {
        auto entry = std::make_unique<juce::DynamicObject>();
        entry->setProperty ("name", name);
        entry->setProperty ("category", category);
        entry->setProperty ("isFactory", isFactory);
        return juce::var (entry.release());
    }

    juce::var presetStatus (const PresetManager& presets)
    {
        const auto current = presets.getCurrent();
        auto status = std::make_unique<juce::DynamicObject>();
        status->setProperty ("current", toVar (current.name, current.category, current.isFactory));
        status->setProperty ("modified", presets.isModified());
        return juce::var (status.release());
    }

    std::optional<PresetManager::Preset> findPreset (const PresetManager& presets, const juce::var& entry)
    {
        return presets.find (entry["name"].toString(), entry["isFactory"]);
    }

    // The Mac web view is white until the page draws, and JUCE has no option for it
    void hideMacWebViewBackground ([[maybe_unused]] juce::WebBrowserComponent& browser)
    {
       #if JUCE_MAC
        for (auto* child : browser.getChildren())
            if (auto* host = dynamic_cast<juce::NSViewComponent*> (child))
                [(id) host->getView() setValue: @NO forKey: @"drawsBackground"];
       #endif
    }
}

//==============================================================================
WebViewEditor::WebViewEditor (juce::AudioProcessor& pluginProcessor, const Plugin& pluginToShow)
    : AudioProcessorEditor (&pluginProcessor),
      plugin (pluginToShow),
      uiZipStream (WebUIData::WebUI_zip, static_cast<size_t> (WebUIData::WebUI_zipSize), false)
{
    for (const auto* spec : plugin.specs)
        relays.push_back (std::make_unique<juce::WebSliderRelay> (spec->id));

    lastPresetChange = plugin.presets.getChangeCounter();

    if (plugin.activation != nullptr)
    {
        activation = std::make_unique<ActivationActions> (*plugin.activation);
        activationBarShowing = activation->needsActivation();
    }

    browser = std::make_unique<SinglePageBrowser> (createOptions());
    hideMacWebViewBackground (*browser);

    for (size_t i = 0; i < relays.size(); ++i)
    {
        auto* parameter = plugin.parameters.getParameter (plugin.specs[i]->id);

        // Every spec needs its parameter
        jassert (parameter != nullptr);

        if (parameter != nullptr)
            attachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (*parameter, *relays[i], nullptr));
    }

    // Drop anything played while no meter was showing
    plugin.meter.reset();

    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    jassert (plugin.width > 0 && plugin.height > 0);
    setSize (plugin.width, plugin.height);
    startTimerHz (LevelMeter::Ballistics::updateHz);
}

juce::WebBrowserComponent::Options WebViewEditor::createOptions()
{
    auto options = juce::WebBrowserComponent::Options{}
                       .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                       .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                    .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory))
                                                    .withBackgroundColour (background))
                       .withNativeIntegrationEnabled()
                       .withResourceProvider ([this] (const auto& url) { return getResource (url); });

    for (auto& relay : relays)
        options = options.withOptionsFrom (*relay);

    return withActivationFunctions (withPresetFunctions (options))
        .withInitialisationData ("uiInfo", juce::String (uiInfoJson (plugin.specs, JucePlugin_VersionString, buildCommit)))
        .withInitialisationData ("presetStatus", juce::JSON::toString (presetStatus (plugin.presets), true))
        .withInitialisationData ("activationStatus", juce::JSON::toString (activationStatus(), true))
        .withInitialisationData ("running", About::running (processor));
}

juce::WebBrowserComponent::Options WebViewEditor::withPresetFunctions (const juce::WebBrowserComponent::Options& options)
{
    using Args = juce::Array<juce::var>;
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    auto& presets = plugin.presets;

    const auto userPresetCall = [&presets] (auto action)
    {
        return [&presets, action] (const Args& args, Completion complete)
        {
            const auto preset = findPreset (presets, args[0]);
            complete (preset.has_value() ? action (*preset, args).getErrorMessage() : juce::String ("That preset no longer exists."));
        };
    };

    return options
        .withNativeFunction ("presetStatus", [&presets] (const Args&, Completion complete)
        {
            complete (presetStatus (presets));
        })
        .withNativeFunction ("presetList", [&presets] (const Args&, Completion complete)
        {
            presets.refreshUserPresets();

            juce::Array<juce::var> list;

            for (const auto& preset : presets.getPresets())
                list.add (toVar (preset.name, preset.category, preset.isFactory));

            complete (list);
        })
        .withNativeFunction ("presetCurrent", [&presets] (const Args&, Completion complete)
        {
            const auto preset = presets.getCurrentPreset();
            complete (preset.has_value() ? toVar (preset->name, preset->category, preset->isFactory) : juce::var());
        })
        .withNativeFunction ("presetLoad", [&presets] (const Args& args, Completion complete)
        {
            const auto preset = findPreset (presets, args[0]);
            complete (preset.has_value() && presets.loadPreset (*preset));
        })
        .withNativeFunction ("presetInit", [&presets] (const Args&, Completion complete)
        {
            presets.loadInit();
            complete ({});
        })
        .withNativeFunction ("presetStep", [&presets] (const Args& args, Completion complete)
        {
            if (static_cast<int> (args[0]) > 0)
                presets.loadNext();
            else
                presets.loadPrevious();

            complete ({});
        })
        .withNativeFunction ("presetExists", [&presets] (const Args& args, Completion complete)
        {
            complete (presets.userPresetExists (args[0].toString()));
        })
        .withNativeFunction ("presetSave", [&presets] (const Args& args, Completion complete)
        {
            complete (presets.saveUserPreset (args[0].toString(), args[1].toString()).getErrorMessage());
        })
        .withNativeFunction ("presetRename", userPresetCall ([&presets] (const PresetManager::Preset& preset, const Args& args)
        {
            return presets.renameUserPreset (preset, args[1].toString());
        }))
        .withNativeFunction ("presetDelete", userPresetCall ([&presets] (const PresetManager::Preset& preset, const Args&)
        {
            return presets.deleteUserPreset (preset);
        }))
        .withNativeFunction ("presetShowFolder", [&presets] (const Args&, Completion complete)
        {
            presets.showUserDirectory();
            complete ({});
        });
}

juce::var WebViewEditor::activationStatus() const
{
    auto status = std::make_unique<juce::DynamicObject>();
    status->setProperty ("show", activation != nullptr && activation->needsActivation());
    status->setProperty ("machine", activation != nullptr ? activation->machineCode() : juce::String());
    return juce::var (status.release());
}

juce::WebBrowserComponent::Options WebViewEditor::withActivationFunctions (const juce::WebBrowserComponent::Options& options)
{
    using Args = juce::Array<juce::var>;
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

    if (activation == nullptr)
        return options;

    // Paste and import complete with a message for the user, empty when there is nothing to say
    return options
        .withNativeFunction ("activationStatus", [this] (const Args&, Completion complete)
        {
            complete (activationStatus());
        })
        .withNativeFunction ("activationOpenBrowser", [this] (const Args&, Completion complete)
        {
            activation->openBrowser();
            complete ({});
        })
        .withNativeFunction ("activationPaste", [this] (const Args&, Completion complete)
        {
            complete (activation->paste());
        })
        .withNativeFunction ("activationImport", [this] (const Args&, Completion complete)
        {
            activation->importFile ([completeImport = std::move (complete)] (const juce::String& message) { completeImport (message); });
        });
}

std::optional<juce::WebBrowserComponent::Resource> WebViewEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);

    if (auto* entry = uiZip.getEntry (path))
    {
        const std::unique_ptr<juce::InputStream> stream (uiZip.createStreamForEntry (*entry));

        if (stream != nullptr)
            return juce::WebBrowserComponent::Resource { readAll (*stream), mimeTypeFor (path) };
    }

    return std::nullopt;
}

void WebViewEditor::timerCallback()
{
    juce::Array<juce::var> peaks;

    for (int channel = 0; channel < plugin.meter.getNumChannels(); ++channel)
        peaks.add (plugin.meter.readAndReset (channel));

    auto level = std::make_unique<juce::DynamicObject>();
    level->setProperty ("peaks", peaks);
    browser->emitEventIfBrowserIsVisible ("level", juce::var (level.release()));

    if (const auto change = plugin.presets.getChangeCounter(); change != lastPresetChange && browser->isShowing())
    {
        lastPresetChange = change;
        browser->emitEventIfBrowserIsVisible ("presetStatus", presetStatus (plugin.presets));
    }

    // Once a second, in case another instance activated this plugin
    if (activationBarShowing && ++ticksSinceActivationCheck >= LevelMeter::Ballistics::updateHz && browser->isShowing())
    {
        ticksSinceActivationCheck = 0;
        activation->refresh();

        if (! activation->needsActivation())
        {
            activationBarShowing = false;
            browser->emitEventIfBrowserIsVisible ("activationStatus", activationStatus());
        }
    }
}

void WebViewEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
}

void WebViewEditor::resized()
{
    browser->setBounds (getLocalBounds());
}
