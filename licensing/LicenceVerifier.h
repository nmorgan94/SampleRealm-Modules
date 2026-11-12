namespace licensing
{
    enum class LicenceStatus
    {
        valid,
        malformed,       // not two strict base64url parts, or the signature isn't 64 bytes
        signature,       // no trusted key signed it
        payload,         // signed, but not a v1 payload for the key that signed it
        wrongMachine,    // a good licence for another machine or another plugin
        noLicenceFile,
        noMachineCode    // the system gave no device ID to compare with
    };

    struct LicencePayload
    {
        int version = 0;
        juce::String keyId, machine, tier, issued;
    };

    struct LicenceCheck
    {
        LicenceStatus status;
        std::optional<LicencePayload> payload;   // for valid and wrongMachine
    };

    // The licence format's "Verifying" steps, in their order
    LicenceCheck verifyLicence (std::string_view licenceText,
                                const MachineCode& thisMachine,
                                std::span<const TrustedKey> trustedKeys);
}
