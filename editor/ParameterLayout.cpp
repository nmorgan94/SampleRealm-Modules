namespace
{
    std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ParameterSpecs::Spec& spec, int versionHint)
    {
        const juce::ParameterID id { spec.id, versionHint };

        switch (spec.kind)
        {
            case ParameterSpecs::Kind::integer:
                return std::make_unique<juce::AudioParameterInt> (
                    id, spec.name, juce::roundToInt (spec.min), juce::roundToInt (spec.max),
                    juce::roundToInt (spec.defaultValue),
                    juce::AudioParameterIntAttributes().withLabel (spec.unit));

            case ParameterSpecs::Kind::choice:
                return std::make_unique<juce::AudioParameterChoice> (
                    id, spec.name, juce::StringArray (spec.choices.data(), static_cast<int> (spec.choices.size())),
                    juce::roundToInt (spec.defaultValue));

            case ParameterSpecs::Kind::real:
                break;
        }

        return std::make_unique<juce::AudioParameterFloat> (
            id, spec.name, juce::NormalisableRange<float> (spec.min, spec.max, spec.step, spec.skew),
            spec.defaultValue, juce::AudioParameterFloatAttributes().withLabel (spec.unit));
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (std::span<const ParameterSpecs::Spec* const> specs,
                                                                           int versionHint)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto* spec : specs)
        layout.add (makeParameter (*spec, versionHint));

    return layout;
}
