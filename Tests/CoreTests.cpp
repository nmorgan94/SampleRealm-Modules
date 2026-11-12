#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/ParameterValues.h>
#include <samplerealm/PeakMeter.h>
#include <samplerealm/Transport.h>
#include <samplerealm/UiInfoJson.h>

#include "PluginTestHelpers.h"

using Catch::Matchers::WithinAbs;
using TestHelpers::makeConstantBuffer;

namespace
{
    // A made-up plugin's parameters
    namespace Specs
    {
        using namespace ParameterSpecs;

        constexpr std::array<const char*, 3> shapeNames { "Sine", "Saw", "A \"square\"" };

        constexpr Spec gain { "gain", "Gain", Kind::real, -60.0f, 6.0f, -1.5f, 0.1f, 0.25f, "dB" };
        constexpr Spec voices = integer ("voices", "Voices", 1, 8, 4);
        constexpr Spec shape = choice ("shape", "Shape", shapeNames, 1);

        constexpr std::array all { &gain, &voices, &shape };
    }

    struct FakePlayHead : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override   { return position; }

        juce::Optional<PositionInfo> position;
    };
}

// Beside Transport, where Catch2 looks for it
static bool operator== (const Transport& a, const Transport& b)
{
    return juce::exactlyEqual (a.bpm, b.bpm) && a.playing == b.playing && juce::exactlyEqual (a.ppq, b.ppq);
}

//==============================================================================
TEST_CASE ("Integer and choice specs take whole steps, and a choice runs to its last name", "[core][parameters]")
{
    using namespace ParameterSpecs;

    CHECK (areValid (Specs::all));
    CHECK_THAT (Specs::shape.max, WithinAbs (2.0, 0.0));

    constexpr Spec steppedByHalves { "voices", "Voices", Kind::integer, 1.0f, 8.0f, 4.0f, 0.5f };
    constexpr Spec oneChoiceShort { "shape", "Shape", Kind::choice, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, "", Specs::shapeNames };

    CHECK_FALSE (areValid (std::array { &Specs::gain, &steppedByHalves }));
    CHECK_FALSE (areValid (std::array { &oneChoiceShort }));
}

TEST_CASE ("A toggle is 0 or 1, automatable unless it says otherwise", "[core][parameters]")
{
    using namespace ParameterSpecs;

    constexpr auto mute = toggle ("mute", "Mute", true);
    constexpr auto scope = notAutomatable (toggle ("scope", "Scope", false));
    constexpr Spec upToTwo { "mute", "Mute", Kind::toggle, 0.0f, 2.0f, 0.0f, 1.0f };

    CHECK (areValid (std::array { &mute, &scope }));
    constexpr Spec halfOn { "mute", "Mute", Kind::toggle, 0.0f, 1.0f, 0.5f, 1.0f };

    CHECK_FALSE (areValid (std::array { &upToTwo }));
    CHECK_FALSE (areValid (std::array { &halfOn }));

    CHECK_THAT (mute.defaultValue, WithinAbs (1.0, 0.0));
    CHECK (mute.automatable);
    CHECK_FALSE (scope.automatable);
    CHECK (Specs::gain.automatable);

    // Whole, as an integer
    ParameterValues values { std::array { &mute } };
    values.set (0, 0.4f);
    CHECK_THAT (values (mute), WithinAbs (0.0, 0.0));
    values.set (0, 3.0f);
    CHECK_THAT (values (mute), WithinAbs (1.0, 0.0));

    CHECK (uiInfoJson (std::array { &mute }, "0.0.1", "unknown") == std::string (R"({"version":"0.0.1","commit":"unknown","build":")") + BuildType::name
        + R"(","params":[{"id":"mute","name":"Mute","kind":"toggle","min":0,"max":1,"defaultValue":1,"step":1,"skew":1,"unit":"","choices":[]}]})");
}

TEST_CASE ("Parameter values start at their defaults and are set by the host's order", "[core][parameters]")
{
    ParameterValues values { Specs::all };

    CHECK_THAT (values (Specs::gain), WithinAbs (-1.5, 0.0));
    CHECK_THAT (values (Specs::voices), WithinAbs (4.0, 0.0));
    CHECK_THAT (values (Specs::shape), WithinAbs (1.0, 0.0));

    values.set (1, 7.0f);
    CHECK_THAT (values (Specs::voices), WithinAbs (7.0, 0.0));
    CHECK_THAT (values (Specs::gain), WithinAbs (-1.5, 0.0));

    // A position that isn't there, or a NaN, changes nothing
    values.set (-1, 3.0f);
    values.set (3, 3.0f);
    values.set (0, std::nanf (""));
    CHECK_THAT (values (Specs::gain), WithinAbs (-1.5, 0.0));
    CHECK_THAT (values (Specs::voices), WithinAbs (7.0, 0.0));
    CHECK_THAT (values (Specs::shape), WithinAbs (1.0, 0.0));

    // As the APVTS: kept inside the range, and whole for an integer or a choice
    values.set (0, 100.0f);
    CHECK_THAT (values (Specs::gain), WithinAbs (6.0, 0.0));
    values.set (0, -std::numeric_limits<float>::infinity());
    CHECK_THAT (values (Specs::gain), WithinAbs (-60.0, 0.0));
    values.set (0, -3.25f);
    CHECK_THAT (values (Specs::gain), WithinAbs (-3.25, 0.0));
    values.set (1, 2.6f);
    CHECK_THAT (values (Specs::voices), WithinAbs (3.0, 0.0));
    values.set (2, 7.0f);
    CHECK_THAT (values (Specs::shape), WithinAbs (2.0, 0.0));

    // Halves go to the even number, as juce::roundToInt
    values.set (1, 4.5f);
    CHECK_THAT (values (Specs::voices), WithinAbs (4.0, 0.0));
    values.set (1, 5.5f);
    CHECK_THAT (values (Specs::voices), WithinAbs (6.0, 0.0));
    values.set (2, 0.5f);
    CHECK_THAT (values (Specs::shape), WithinAbs (0.0, 0.0));
    values.set (2, 1.5f);
    CHECK_THAT (values (Specs::shape), WithinAbs (2.0, 0.0));

    // A spec that isn't in the list reads as its default
    constexpr ParameterSpecs::Spec other { "other", "Other", ParameterSpecs::Kind::real, 0.0f, 1.0f, 0.5f };
    CHECK_THAT (values (other), WithinAbs (0.5, 0.0));
}

TEST_CASE ("The ui info lists each spec as the web editor reads it", "[core][parameters]")
{
    const auto json = uiInfoJson (Specs::all, "1.2.3", "abc1234");

    CHECK (json == std::string (R"({"version":"1.2.3","commit":"abc1234","build":")") + BuildType::name + R"(","params":[)"
        R"({"id":"gain","name":"Gain","kind":"real","min":-60,"max":6,"defaultValue":-1.5,"step":0.1,"skew":0.25,"unit":"dB","choices":[]},)"
        R"({"id":"voices","name":"Voices","kind":"integer","min":1,"max":8,"defaultValue":4,"step":1,"skew":1,"unit":"","choices":[]},)"
        R"({"id":"shape","name":"Shape","kind":"choice","min":0,"max":2,"defaultValue":1,"step":1,"skew":1,"unit":"","choices":["Sine","Saw","A \"square\""]})"
        "]}");

    CHECK (uiInfoJson ({}, "0.0.1", "unknown") == std::string (R"({"version":"0.0.1","commit":"unknown","build":")") + BuildType::name + R"(","params":[]})");
}

//==============================================================================
TEST_CASE ("Peak meter holds each channel's loudest sample until read", "[core][meter]")
{
    PeakMeter meter { 2 };
    auto buffer = makeConstantBuffer (2, 64, 0.0f);

    buffer.setSample (0, 10, -0.8f);    // a negative peak counts as much as a positive one
    buffer.setSample (1, 3, 0.3f);
    meter.process (buffer);

    // A quieter block afterwards doesn't lower what's waiting to be read
    meter.process (makeConstantBuffer (2, 64, 0.1f));

    CHECK_THAT (meter.readAndReset (0), WithinAbs (0.8, 1.0e-6));
    CHECK_THAT (meter.readAndReset (1), WithinAbs (0.3, 1.0e-6));

    CHECK_THAT (meter.readAndReset (0), WithinAbs (0.0, 0.0));
    CHECK_THAT (meter.readAndReset (1), WithinAbs (0.0, 0.0));
}

TEST_CASE ("A mono peak meter ignores extra channels", "[core][meter]")
{
    PeakMeter meter { 2 };
    meter.setNumChannels (1);
    REQUIRE (meter.getNumChannels() == 1);

    auto buffer = makeConstantBuffer (2, 64, 0.2f);
    buffer.applyGain (1, 0, 64, 4.0f);
    meter.process (buffer);

    CHECK_THAT (meter.readAndReset (0), WithinAbs (0.2, 1.0e-6));
    CHECK_THAT (meter.readAndReset (1), WithinAbs (0.0, 0.0));
}

//==============================================================================
TEST_CASE ("The transport takes the host's tempo and position, or defaults", "[core][transport]")
{
    FakePlayHead host;
    juce::AudioPlayHead::PositionInfo position;
    position.setBpm (128.0);
    position.setIsPlaying (true);
    position.setPpqPosition (8.5);
    host.position = position;

    CHECK (Transport::from (&host) == Transport { 128.0, true, 8.5 });

    // The Standalone has no play head. Hosts may give no position, or leave out the tempo and the beat
    CHECK (Transport::from (nullptr) == Transport {});
    host.position = juce::nullopt;
    CHECK (Transport::from (&host) == Transport {});
    host.position = juce::AudioPlayHead::PositionInfo {};
    CHECK (Transport::from (&host) == Transport {});
}

TEST_CASE ("The transport moves on only while playing", "[core][transport]")
{
    Transport transport { 120.0, true, 4.0 };

    // Half a second at 120 BPM is one beat
    transport.advance (24000, 48000.0);
    CHECK_THAT (transport.ppq, WithinAbs (5.0, 1.0e-12));

    transport.playing = false;
    transport.advance (24000, 48000.0);
    CHECK_THAT (transport.ppq, WithinAbs (5.0, 1.0e-12));
}
