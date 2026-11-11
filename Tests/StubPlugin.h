#pragma once

#include <editor/editor.h>

//==============================================================================
/** A plugin with one parameter and no sound, for these tests. */
namespace StubPlugin
{
    inline constexpr int versionHint = 1;

    inline constexpr ParameterSpecs::Spec gain { "gain", "Gain", ParameterSpecs::Kind::real, -24.0f, 24.0f, 0.0f, 0.1f, 1.0f, "dB" };
    inline constexpr std::array all { &gain };

    inline const juce::ParameterID gainId { gain.id, versionHint };

    struct SilentProcessor : juce::AudioProcessor
    {
        const juce::String getName() const override   { return "Test"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override   { return 0.0; }
        bool acceptsMidi() const override   { return false; }
        bool producesMidi() const override   { return false; }
        juce::AudioProcessorEditor* createEditor() override   { return nullptr; }
        bool hasEditor() const override   { return false; }
        int getNumPrograms() override   { return 1; }
        int getCurrentProgram() override   { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override   { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    class Processor : public SilentProcessor
    {
    public:
        juce::AudioProcessorValueTreeState& getAPVTS()   { return apvts; }

    private:
        juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout (all, versionHint) };
    };
}
