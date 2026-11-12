#pragma once

//==============================================================================
/** A JUCE parameter for each spec, in the order given. Every id carries the version hint. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (std::span<const ParameterSpecs::Spec* const> specs,
                                                                           int versionHint);
