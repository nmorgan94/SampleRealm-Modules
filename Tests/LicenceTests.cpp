
#include "LicenceFixtures.h"

using namespace licensing;
using namespace LicenceFixtures;

namespace
{
    // The names cases.json uses
    juce::String nameOf (LicenceStatus status)
    {
        switch (status)
        {
            case LicenceStatus::valid:           return "valid";
            case LicenceStatus::malformed:       return "malformed";
            case LicenceStatus::signature:       return "signature";
            case LicenceStatus::payload:         return "payload";
            case LicenceStatus::wrongMachine:    return "wrong machine";
            case LicenceStatus::noLicenceFile:   return "no licence file";
            case LicenceStatus::noMachineCode:   return "no machine code";
        }

        return {};
    }
}

//==============================================================================
TEST_CASE ("Every licence fixture gives the result cases.json expects", "[licence]")
{
    const TestKey testKey;
    const auto machine = fixtureMachine();
    const auto casesJson = fixtureJson ("cases.json");
    const auto* cases = casesJson["cases"].getArray();

    REQUIRE (cases != nullptr);
    REQUIRE (cases->size() == 7);

    for (const auto& row : *cases)
    {
        const auto fileName = row["file"].toString();
        CAPTURE (fileName);

        const auto check = verifyLicence (fixtureText (fileName), machine, testKey.trusted());
        CHECK (nameOf (check.status) == row["expect"].toString());

        const auto expectedPayload = row["payload"];
        CHECK (check.payload.has_value() == expectedPayload.isObject());

        if (check.payload && expectedPayload.isObject())
        {
            CHECK (juce::var (check.payload->version) == expectedPayload["v"]);
            CHECK (check.payload->keyId   == expectedPayload["keyId"].toString());
            CHECK (check.payload->machine == expectedPayload["machine"].toString());
            CHECK (check.payload->tier    == expectedPayload["tier"].toString());
            CHECK (check.payload->issued  == expectedPayload["issued"].toString());
        }
    }
}

TEST_CASE ("A valid licence for another machine reports wrong machine", "[licence]")
{
    const TestKey testKey;
    const auto anotherMachine = MachineCode::fromDeviceId ("another machine", "Example/machine-code/v1");
    REQUIRE (anotherMachine);

    const auto check = verifyLicence (fixtureText ("valid.licence"), *anotherMachine, testKey.trusted());

    CHECK (check.status == LicenceStatus::wrongMachine);
    REQUIRE (check.payload);
    CHECK (check.payload->machine == fixtureMachine().canonical());
}

TEST_CASE ("A valid licence for another plugin on the same machine reports wrong machine", "[licence]")
{
    const TestKey testKey;
    const juce::String deviceId { "one machine" };
    const auto codeForOnePlugin = MachineCode::fromDeviceId (deviceId, "Example/One-Plugin/machine-code/v1");
    const auto codeForAnotherPlugin = MachineCode::fromDeviceId (deviceId, "Example/Another-Plugin/machine-code/v1");
    REQUIRE (codeForOnePlugin);
    REQUIRE (codeForAnotherPlugin);

    CHECK (codeForOnePlugin != codeForAnotherPlugin);

    // The fixture's machine stands for the plugin the licence was issued to
    const auto licence = fixtureText ("valid.licence");
    CHECK (verifyLicence (licence, fixtureMachine(), testKey.trusted()).status == LicenceStatus::valid);
    CHECK (verifyLicence (licence, *codeForAnotherPlugin, testKey.trusted()).status == LicenceStatus::wrongMachine);
}

TEST_CASE ("A missing licence file reports no licence file", "[licence]")
{
    const TestKey testKey;
    const juce::TemporaryFile missing (".licence");
    const auto check = checkLicenceFile (missing.getFile(), fixtureMachine(), testKey.trusted());

    CHECK (check.status == LicenceStatus::noLicenceFile);
}

TEST_CASE ("Whitespace around the licence in its file is tolerated", "[licence]")
{
    const TestKey testKey;
    const juce::TemporaryFile temporary (".licence");
    const auto licenceFile = temporary.getFile();
    const auto resaved = "\xEF\xBB\xBF \r\n\t" + fixtureText ("valid.licence") + " \t\r\n\r\n";

    REQUIRE (licenceFile.replaceWithData (resaved.data(), resaved.size()));

    CHECK (checkLicenceFile (licenceFile, fixtureMachine(), testKey.trusted()).status == LicenceStatus::valid);
}

//==============================================================================
TEST_CASE ("The machine code is canonical and the same on every call", "[licence]")
{
    const auto machine = MachineCode::forThisMachine();
    REQUIRE (machine);

    const auto code = machine->canonical();
    CAPTURE (code);

    CHECK (code.length() == 16);
    CHECK (code.containsOnly ("0123456789ABCDEFGHJKMNPQRSTVWXYZ"));
    CHECK (MachineCode::fromCanonical (code) == machine);
    CHECK (MachineCode::forThisMachine() == machine);

    const auto grouped = machine->grouped();
    CHECK (grouped.length() == 19);
    CHECK (juce::StringArray::fromTokens (grouped, "-", "").size() == 4);
    CHECK (grouped.removeCharacters ("-") == code);
    CHECK (grouped.substring (0, 4) == code.substring (0, 4));
}

// Computed apart from this code: Python's hmac and hashlib. Every plugin must give this answer
TEST_CASE ("The machine code recipe gives its known answer", "[licence]")
{
    const auto machine = MachineCode::fromDeviceId ("0123456789ABCDEF0123456789ABCDEF", "Example/machine-code/v1");
    REQUIRE (machine);

    CHECK (machine->canonical() == "51A68FERHN2922BP");
    CHECK (machine->grouped() == "51A6-8FER-HN29-22BP");
    CHECK_FALSE (MachineCode::fromDeviceId ("", "Example/machine-code/v1").has_value());
}
