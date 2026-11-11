#pragma once

#include <catch2/catch_test_macros.hpp>

#include <licensing/licensing.h>

//==============================================================================
namespace LicenceFixtures
{
    inline juce::File fixtureFile (const juce::String& fileName)
    {
        const auto file = juce::File (LICENCE_FIXTURES_DIR).getChildFile (fileName);
        REQUIRE (file.existsAsFile());
        return file;
    }

    inline std::string fixtureText (const juce::String& fileName)   { return fixtureFile (fileName).loadFileAsString().toStdString(); }
    inline juce::var fixtureJson (const juce::String& fileName)     { return juce::JSON::parse (fixtureFile (fileName)); }

    // The website's test key, which only the tests may trust
    struct TestKey
    {
        TestKey()
        {
            const auto json = fixtureJson ("test-key.json");
            keyId = json["keyId"].toString().toStdString();

            const auto parsedKey = licensing::publicKeyFromHex (json["publicKey"].toString().toStdString());
            REQUIRE (parsedKey);
            publicKey = *parsedKey;
        }

        std::vector<licensing::TrustedKey> trusted() const   { return { { keyId, publicKey } }; }

        std::string keyId;
        licensing::PublicKey publicKey {};
    };

    inline licensing::MachineCode fixtureMachine()
    {
        const auto machine = licensing::MachineCode::fromCanonical (fixtureJson ("cases.json")["machine"].toString());
        REQUIRE (machine);
        return *machine;
    }

    // An Activation for the fixtures' machine, trusting the test key, with a licence file of its own
    struct ActivationFixture
    {
        TestKey testKey;
        std::vector<licensing::TrustedKey> trusted = testKey.trusted();
        juce::TemporaryFile temporary { ".licence" };
        juce::File licenceFile = temporary.getFile();
        licensing::Activation activation { fixtureMachine(), trusted, licenceFile };
    };
}
