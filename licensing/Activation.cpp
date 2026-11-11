namespace licensing
{
    static juce::String activationPage()
    {
        return { config::activationUrl.data(), config::activationUrl.size() };
    }

    Activation::Activation()
        : Activation (MachineCode::forThisMachine(), productionKeys(), licenceFileLocation())
    {
    }

    Activation::Activation (std::optional<MachineCode> machineToActivate, std::span<const TrustedKey> keysToTrust, juce::File fileToKeep)
        : thisMachine (std::move (machineToActivate)), trustedKeys (keysToTrust.begin(), keysToTrust.end()), licenceFile (std::move (fileToKeep))
    {
        refresh();
    }

    Activation::~Activation() = default;

    LicenceStatus Activation::refresh()
    {
        const juce::ScopedLock lock (fileLock);

        lastStatus = thisMachine ? checkLicenceFile (licenceFile, *thisMachine, trustedKeys).status
                                 : LicenceStatus::noMachineCode;
        return lastStatus;
    }

    LicenceStatus Activation::install (std::string_view licenceText)
    {
        if (! thisMachine)
            return LicenceStatus::noMachineCode;

        // Or the file written here would be refused when read back
        if (licenceText.size() > largestLicenceBytes)
            return LicenceStatus::malformed;

        const auto offered = verifyLicence (licenceText, *thisMachine, trustedKeys).status;

        if (offered != LicenceStatus::valid)
            return offered;

        const auto licence = trimWhitespace (licenceText);
        const juce::ScopedLock lock (fileLock);

        if (! licenceFile.getParentDirectory().createDirectory()
            || ! licenceFile.replaceWithData (licence.data(), licence.size()))
            return LicenceStatus::noLicenceFile;

        lastStatus = LicenceStatus::valid;
        return LicenceStatus::valid;
    }

    juce::String Activation::pageAddress() const
    {
        return thisMachine ? activationPage() + "?machine=" + thisMachine->canonical() : activationPage();
    }

    juce::String Activation::pageAddress (int port, const juce::String& state) const
    {
        return pageAddress() + "&port=" + juce::String (port) + "&state=" + state;
    }

    juce::String Activation::beginInBrowser()
    {
        if (! thisMachine)
            return pageAddress();

        if (receiver == nullptr)
            receiver = std::make_unique<LoopbackReceiver> (*this);

        const auto state = LoopbackReceiver::newState();
        const auto port = receiver->start (state);

        return port ? pageAddress (*port, state) : pageAddress();
    }

    juce::String Activation::finishedAddress (LicenceStatus installed)
    {
        return activationPage() + (installed == LicenceStatus::valid ? "/done" : "/done?result=rejected");
    }
}
