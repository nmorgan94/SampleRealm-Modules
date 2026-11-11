// Built into every plugin's tests by licensing::fixtures: they check that plugin's own build

#include "LicenceFixtures.h"

using namespace licensing;
using namespace LicenceFixtures;

//==============================================================================
TEST_CASE ("The salt and the licence file take the company's name", "[licence]")
{
    const juce::String company { JucePlugin_Manufacturer };

    CHECK (MachineCode::companySalt() == (company + "/machine-code/v1").toStdString());
    CHECK (MachineCode::forThisMachine()
             == MachineCode::fromDeviceId (juce::SystemStats::getUniqueDeviceID(), MachineCode::companySalt()));

    const auto location = licenceFileLocation();
    CHECK (location.getFileName() == company + ".licence");
    CHECK (location.getParentDirectory().getFileName() == company);
}

//==============================================================================
TEST_CASE ("A plugin build trusts only the keys CMake gives it, never the test key", "[licence]")
{
    const TestKey testKey;
    const auto keys = productionKeys();
    juce::StringArray trusted;

    for (const auto& key : keys)
    {
        CHECK (key.keyId != testKey.keyId);
        CHECK (key.publicKey != testKey.publicKey);

        trusted.add (juce::String (key.keyId.data(), key.keyId.size()) + ":"
                       + juce::String::toHexString (key.publicKey.data(), static_cast<int> (key.publicKey.size()), 0));
    }

    CHECK (trusted.joinIntoString (",") == LICENCE_KEYS);

    const auto check = verifyLicence (fixtureText ("valid.licence"), fixtureMachine(), keys);
    CHECK (check.status == LicenceStatus::signature);
}
