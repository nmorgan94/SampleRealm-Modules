namespace licensing
{
    // This plugin's own licence file: <COMPANY_NAME>/<PRODUCT_NAME>.licence
    juce::File licenceFileLocation();

    // Nothing longer is read, installed or imported
    inline constexpr std::size_t largestLicenceBytes = 4096;

    LicenceCheck checkLicenceFile (const juce::File& licenceFile,
                                   const MachineCode& thisMachine,
                                   std::span<const TrustedKey> trustedKeys);
}
