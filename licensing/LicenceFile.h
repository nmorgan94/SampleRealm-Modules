namespace licensing
{
    // The one licence file every plugin of the company reads: <COMPANY_NAME>/<COMPANY_NAME>.licence
    juce::File licenceFileLocation();

    // Nothing longer is read, installed or imported
    inline constexpr std::size_t largestLicenceBytes = 4096;

    LicenceCheck checkLicenceFile (const juce::File& licenceFile,
                                   const MachineCode& thisMachine,
                                   std::span<const TrustedKey> trustedKeys);
}
