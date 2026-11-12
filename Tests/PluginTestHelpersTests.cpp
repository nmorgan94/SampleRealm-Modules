#include <catch2/catch_test_macros.hpp>

#include "PluginTestHelpers.h"
#include "StubPlugin.h"

namespace
{
    struct MidiRecorder : StubPlugin::SilentProcessor
    {
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midi) override
        {
            for (const auto event : midi)
                received.emplace_back (event.getMessage(), event.samplePosition);
        }

        std::vector<std::pair<juce::MidiMessage, int>> received;
    };
}

//==============================================================================
TEST_CASE ("A block is processed with no MIDI, or with one message at its start", "[helpers]")
{
    MidiRecorder processor;
    auto buffer = TestHelpers::makeConstantBuffer (2, TestHelpers::blockSize, 0.0f);

    TestHelpers::process (processor, buffer);
    CHECK (processor.received.empty());

    TestHelpers::process (processor, buffer, juce::MidiMessage::noteOn (1, 60, 0.5f));
    REQUIRE (processor.received.size() == 1);
    CHECK (processor.received[0].first.isNoteOn());
    CHECK (processor.received[0].first.getNoteNumber() == 60);
    CHECK (processor.received[0].second == 0);
}
