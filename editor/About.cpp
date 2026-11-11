#include "About.h"
#include <samplerealm/BuildCommit.h>
#include <samplerealm/BuildType.h>

namespace About
{
    juce::String running (const juce::AudioProcessor& processor)
    {
        const juce::String format (juce::AudioProcessor::getWrapperTypeDescription (processor.wrapperType));
        const juce::PluginHostType host;

        return host.type == juce::PluginHostType::UnknownHost ? format : format + " in " + host.getHostDescription();
    }

    juce::String text (const juce::AudioProcessor& processor)
    {
        return juce::StringArray { "Version " JucePlugin_VersionString,
                                   juce::String (BuildType::name) + " build",
                                   "Commit " + juce::String (buildCommit),
                                   running (processor),
                                   "Native UI" }.joinIntoString ("\n");
    }
}
