namespace licensing
{
    class LoopbackReceiver;

    /**
     * Whether this machine is activated, and how a licence gets installed.
     * Reads and writes the licence file, so never for the audio thread.
     */
    class Activation
    {
    public:
        // This machine, productionKeys() and licenceFileLocation()
        Activation();

        // For tests, which have no licence for the machine they run on
        Activation (std::optional<MachineCode> machineToActivate, std::span<const TrustedKey> keysToTrust, juce::File fileToKeep);

        ~Activation();

        const std::optional<MachineCode>& machine() const   { return thisMachine; }

        // As last read or installed
        LicenceStatus status() const   { return lastStatus; }
        bool isActivated() const       { return status() == LicenceStatus::valid; }

        // Reads the licence file again
        LicenceStatus refresh();

        // Keeps the licence only if it is valid for this machine. Returns what it was
        LicenceStatus install (std::string_view licenceText);

        // The website's page for this machine. The port and state go with a listening LoopbackReceiver
        juce::String pageAddress() const;
        juce::String pageAddress (int port, const juce::String& state) const;

        // The address to open in the browser, after starting to listen for its answer where the host allows
        juce::String beginInBrowser();

        // Where the receiver sends the browser once it has a licence
        static juce::String finishedAddress (LicenceStatus installed);

    private:
        const std::optional<MachineCode> thisMachine;
        const std::vector<TrustedKey> trustedKeys;
        const juce::File licenceFile;

        juce::CriticalSection fileLock;
        std::atomic<LicenceStatus> lastStatus { LicenceStatus::noLicenceFile };
        std::unique_ptr<LoopbackReceiver> receiver;

        JUCE_DECLARE_NON_COPYABLE (Activation)
    };
}
