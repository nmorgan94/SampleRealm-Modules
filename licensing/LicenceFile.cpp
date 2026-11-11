namespace licensing
{
    juce::File licenceFileLocation()
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif

        const juce::String company (config::companyName.data(), config::companyName.size());

        return base.getChildFile (company).getChildFile (company + ".licence");
    }

    LicenceCheck checkLicenceFile (const juce::File& licenceFile,
                                   const MachineCode& thisMachine,
                                   std::span<const TrustedKey> trustedKeys)
    {
        if (! licenceFile.existsAsFile())
            return { LicenceStatus::noLicenceFile, {} };

        juce::MemoryBlock contents;

        if (std::cmp_greater (licenceFile.getSize(), largestLicenceBytes) ||! licenceFile.loadFileAsData (contents))
            return { LicenceStatus::malformed, {} };

        return verifyLicence ({ contents.begin(), contents.getSize() }, thisMachine, trustedKeys);
    }
}
