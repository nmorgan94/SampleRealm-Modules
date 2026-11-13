#pragma once

//==============================================================================
/** Changes a real spec's attributes, for value text of the plugin's own: returns them as given for any other spec. */
using FloatAttributesFor = std::function<juce::AudioParameterFloatAttributes (const ParameterSpecs::Spec&, juce::AudioParameterFloatAttributes)>;

/** A JUCE parameter for each spec, in the order given. Every id carries the version hint. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (std::span<const ParameterSpecs::Spec* const> specs,
                                                                           int versionHint,
                                                                           const FloatAttributesFor& floatAttributesFor = nullptr);
