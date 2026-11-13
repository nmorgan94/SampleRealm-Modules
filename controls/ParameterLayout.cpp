namespace
{
    std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ParameterSpecs::Spec& spec, int versionHint,
                                                               const FloatAttributesFor& floatAttributesFor)
    {
        const juce::ParameterID id { spec.id, versionHint };

        switch (spec.kind)
        {
            case ParameterSpecs::Kind::integer:
                return std::make_unique<juce::AudioParameterInt> (
                    id, spec.name, juce::roundToInt (spec.min), juce::roundToInt (spec.max),
                    juce::roundToInt (spec.defaultValue),
                    juce::AudioParameterIntAttributes().withLabel (spec.unit).withAutomatable (spec.automatable));

            case ParameterSpecs::Kind::choice:
            {
                const juce::StringArray choices (spec.choices.data(), static_cast<int> (spec.choices.size()));

                // As web/editor/params.ts parseValue: a name in any case, then a number
                const auto indexFromText = [choices] (const juce::String& text)
                {
                    for (int index = 0; index < choices.size(); ++index)
                        if (text.trim().equalsIgnoreCase (choices[index]))
                            return index;

                    return juce::roundToInt (text.getDoubleValue());
                };

                return std::make_unique<juce::AudioParameterChoice> (
                    id, spec.name, choices, juce::roundToInt (spec.defaultValue),
                    juce::AudioParameterChoiceAttributes().withValueFromStringFunction (indexFromText)
                                                           .withAutomatable (spec.automatable));
            }

            case ParameterSpecs::Kind::toggle:
                return std::make_unique<juce::AudioParameterBool> (
                    id, spec.name, spec.defaultValue >= 0.5f,
                    juce::AudioParameterBoolAttributes().withLabel (spec.unit).withAutomatable (spec.automatable));

            case ParameterSpecs::Kind::real:
                break;
        }

        auto attributes = juce::AudioParameterFloatAttributes().withLabel (spec.unit).withAutomatable (spec.automatable);

        if (floatAttributesFor != nullptr)
            attributes = floatAttributesFor (spec, attributes);

        return std::make_unique<juce::AudioParameterFloat> (
            id, spec.name, juce::NormalisableRange<float> (spec.min, spec.max, spec.step, spec.skew),
            spec.defaultValue, attributes);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (std::span<const ParameterSpecs::Spec* const> specs,
                                                                           int versionHint,
                                                                           const FloatAttributesFor& floatAttributesFor)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto* spec : specs)
        layout.add (makeParameter (*spec, versionHint, floatAttributesFor));

    return layout;
}
