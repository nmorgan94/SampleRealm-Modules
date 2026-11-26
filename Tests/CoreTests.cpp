#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/ParameterValues.h>
#include <samplerealm/PeakMeter.h>
#include <samplerealm/SpectrumAnalyser.h>
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
    return juce::exactlyEqual (a.bpm, b.bpm) && a.playing == b.playing && juce::exactlyEqual (a.ppq, b.ppq)
        && a.available == b.available && a.hostPlaying == b.hostPlaying;
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

TEST_CASE ("A centre lies inside a real spec's range, in place of a skew, and the web editor gets its skew", "[core][parameters]")
{
    using namespace ParameterSpecs;

    constexpr auto tone = withCentre ({ "tone", "Tone", Kind::real, 200.0f, 16000.0f, 4000.0f }, 3000.0f);
    constexpr auto atTheEdge = withCentre ({ "tone", "Tone", Kind::real, 200.0f, 16000.0f, 4000.0f }, 200.0f);
    constexpr auto skewedToo = withCentre ({ "tone", "Tone", Kind::real, 200.0f, 16000.0f, 4000.0f, 0.0f, 0.3f }, 3000.0f);
    constexpr auto wholeSteps = withCentre (integer ("voices", "Voices", 1, 8, 4), 2.0f);

    CHECK (areValid (std::array { &tone }));
    CHECK_FALSE (areValid (std::array { &atTheEdge }));
    CHECK_FALSE (areValid (std::array { &skewedToo }));
    CHECK_FALSE (areValid (std::array { &wholeSteps }));

    // log (0.5) / log (60 / 72)
    constexpr auto aboutZero = withCentre ({ "gain", "Gain", Kind::real, -60.0f, 12.0f, 0.0f }, 0.0f);
    CHECK (areValid (std::array { &aboutZero }));
    CHECK_THAT (skewOf (aboutZero), WithinAbs (3.8018, 0.0001));

    // log (0.5) / log (2800 / 15800), to seven significant digits
    CHECK (uiInfoJson (std::array { &tone }, "0.0.1", "unknown") == std::string (R"({"version":"0.0.1","commit":"unknown","build":")") + BuildType::name
        + R"(","params":[{"id":"tone","name":"Tone","kind":"real","min":200,"max":16000,"defaultValue":4000,"step":0,"skew":0.400573,"unit":"","choices":[]}]})");
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

TEST_CASE ("A performance control says so to the web editor, and stays automatable", "[core][parameters]")
{
    using namespace ParameterSpecs;

    constexpr auto bend = performanceControl ({ "bend", "Bend", Kind::real, -1.0f, 1.0f, 0.0f, 0.01f });

    CHECK (bend.isPerformanceControl);
    CHECK (bend.automatable);
    CHECK_FALSE (Specs::gain.isPerformanceControl);

    CHECK (uiInfoJson (std::array { &bend }, "0.0.1", "unknown") == std::string (R"({"version":"0.0.1","commit":"unknown","build":")") + BuildType::name
        + R"(","params":[{"id":"bend","name":"Bend","kind":"real","min":-1,"max":1,"defaultValue":0,"step":0.01,"skew":1,"unit":"","choices":[],"isPerformanceControl":true}]})");
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

    CHECK (Transport::from (&host) == Transport { 128.0, true, 8.5, true, true });

    // The Standalone has no play head. Hosts may give no position, or leave out the tempo and the beat
    CHECK (Transport::from (nullptr) == Transport {});
    host.position = juce::nullopt;
    CHECK (Transport::from (&host) == Transport {});
    host.position = juce::AudioPlayHead::PositionInfo {};
    CHECK (Transport::from (&host) == Transport { 174.0, false, 0.0, true });
}

TEST_CASE ("A host playing with no position counts as stopped, though its own flag says playing", "[core][transport]")
{
    FakePlayHead host;
    juce::AudioPlayHead::PositionInfo position;
    position.setBpm (90.0);
    position.setIsPlaying (true);
    host.position = position;

    CHECK (Transport::from (&host) == Transport { 90.0, false, 0.0, true, true });
}

TEST_CASE ("The transport is available only where a host gives a position", "[core][transport]")
{
    FakePlayHead host;
    host.position = juce::AudioPlayHead::PositionInfo {};
    CHECK (Transport::from (&host).available);

    // As in the Standalone
    host.position = juce::nullopt;
    CHECK_FALSE (Transport::from (&host).available);
    CHECK_FALSE (Transport::from (nullptr).available);
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

//==============================================================================
namespace
{
    using Spectrum = SpectrumAnalyser<96>;

    constexpr double frameSeconds = 1.0 / 60.0;

    size_t bandOf (float hz)
    {
        return static_cast<size_t> (std::floor (Spectrum::numBands * std::log (hz / Spectrum::minHz) / std::log (Spectrum::maxHz / Spectrum::minHz)));
    }

    // A second of a full-scale sine
    std::vector<float> tone (double hz, double rate = TestHelpers::sampleRate)
    {
        std::vector<float> samples (static_cast<size_t> (rate));

        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * hz * static_cast<double> (i) / rate));

        return samples;
    }

    // Half a second of frames, for the bands to reach their levels
    void settle (Spectrum& spectrum)
    {
        for (int i = 0; i < 30; ++i)
            spectrum.update (frameSeconds);
    }

    template <int numBands>
    void push (SpectrumAnalyser<numBands>& spectrum, const std::vector<float>& samples)
    {
        spectrum.push (samples.data(), static_cast<int> (samples.size()));
    }
}

TEST_CASE ("A full-scale 1 kHz sine reads 0 dB in its band and nothing far from it, at any sample rate", "[core][spectrum]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    CAPTURE (rate);

    Spectrum spectrum;
    spectrum.setSampleRate (rate);
    push (spectrum, tone (1000.0, rate));

    settle (spectrum);

    const auto& bands = spectrum.getBands();
    const auto loudest = static_cast<size_t> (std::max_element (bands.begin(), bands.end()) - bands.begin());

    CHECK (loudest == bandOf (1000.0f));
    CHECK_THAT (bands[loudest], WithinAbs (0.0, 1.5));
    CHECK (bands[bandOf (100.0f)] < -60.0f);
    CHECK (bands[bandOf (10000.0f)] < -60.0f);
}

TEST_CASE ("Bands above half the sample rate read nothing", "[core][spectrum]")
{
    // 15 kHz is under half of 32 kHz, and the bands from 16 kHz up are over it
    Spectrum spectrum;
    spectrum.setSampleRate (32000.0);
    push (spectrum, tone (15000.0, 32000.0));
    spectrum.update (1.0);

    CHECK (spectrum.getBands()[bandOf (15000.0f)] > -10.0f);
    CHECK_THAT (spectrum.getBands()[bandOf (17000.0f)], WithinAbs (Spectrum::floorDb, 0.0));
    CHECK_THAT (spectrum.getBands().back(), WithinAbs (Spectrum::floorDb, 0.0));
}

TEST_CASE ("The spectrum tilts up 4.5 dB an octave around 1 kHz", "[core][spectrum]")
{
    // From 65 Hz, where a band is narrower than a bin and is read between two, to 13 kHz
    const auto band = GENERATE (16, 24, 40, 70, 90);
    const auto hz = Spectrum::centreOf (band);
    CAPTURE (band, hz);

    Spectrum spectrum;
    push (spectrum, tone (hz));

    settle (spectrum);

    CHECK_THAT (spectrum.getBands()[static_cast<size_t> (band)], WithinAbs (4.5 * std::log2 (hz / 1000.0), 1.8));
    CHECK_THAT (Spectrum::centreOf (static_cast<int> (bandOf (1000.0f))), WithinAbs (1000.0, 40.0));
}

TEST_CASE ("The spectrum's bands fall slowly, and its peaks hold until they're reset", "[core][spectrum]")
{
    Spectrum spectrum;
    const auto band = bandOf (1000.0f);
    push (spectrum, tone (1000.0));

    settle (spectrum);

    const auto level = spectrum.getBands()[band];

    // A second of silence
    push (spectrum, std::vector<float> (48000, 0.0f));

    for (int i = 0; i < 60; ++i)
        spectrum.update (frameSeconds);

    CHECK_THAT (spectrum.getBands()[band], WithinAbs (level - 24.0f, 0.5));
    CHECK_THAT (spectrum.getPeaks()[band], WithinAbs (level, 1.0e-4));

    spectrum.resetPeaks();
    CHECK_THAT (spectrum.getPeaks()[band], WithinAbs (spectrum.getBands()[band], 0.0));

    for (int i = 0; i < 600; ++i)
        spectrum.update (frameSeconds);

    CHECK_THAT (spectrum.getBands()[band], WithinAbs (Spectrum::floorDb, 0.0));
}

TEST_CASE ("A sample rate that isn't above 0 leaves the spectrum as it was", "[core][spectrum]")
{
    Spectrum spectrum;
    spectrum.setSampleRate (GENERATE (0.0, -48000.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()));
    push (spectrum, tone (1000.0));
    spectrum.update (1.0);

    CHECK_THAT (spectrum.getBands()[bandOf (1000.0f)], WithinAbs (0.0, 1.5));
}

TEST_CASE ("The spectrum has as many bands as it's asked for, over the same range", "[core][spectrum]")
{
    SpectrumAnalyser<24> spectrum;
    push (spectrum, tone (1000.0));
    spectrum.update (1.0);

    const auto& bands = spectrum.getBands();
    const auto loudest = std::max_element (bands.begin(), bands.end()) - bands.begin();

    // 20 Hz to 20 kHz is three decades, so eight bands each
    CHECK (bands.size() == 24);
    CHECK (loudest == 13);
}
