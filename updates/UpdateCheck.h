namespace updates
{
    /**
     * Whether the website has a newer version of this plugin, asked at most once a day.
     * It only tells the user: nothing is downloaded. The contract is the website's docs/update-check.md.
     * Reads and writes a file, so never for the audio thread. Listeners hear when what it knows changes.
     */
    class UpdateCheck : public juce::ChangeBroadcaster,
                        private juce::Thread
    {
    public:
        // The reply's body, or nothing unless the reply was a 200
        using Fetch = std::function<std::optional<juce::String> (const juce::String& address)>;

        struct Config
        {
            // The plugin's page on the website. Empty never asks
            juce::String page;
            juce::String thisVersion;
            juce::File stateFile;

            // Null asks the website
            Fetch fetch = nullptr;

            std::function<juce::Time()> now = [] { return juce::Time::getCurrentTime(); };
        };

        // UPDATE_URL, this build's version and the plugin's own file: <COMPANY_NAME>/<PRODUCT_NAME>.updates
        UpdateCheck();

        // For a plugin's tests: those made after are as in a build with no UPDATE_URL
        static void ignoreUpdateUrl();

        explicit UpdateCheck (Config);

        ~UpdateCheck() override;

        // False in a build with no UPDATE_URL
        bool isConfigured() const                  { return config.page.isNotEmpty(); }

        // Opened for the user. Never an address from a reply
        const juce::String& pageAddress() const    { return config.page; }
        juce::String versionAddress() const        { return config.page + "/version"; }

        // The user's switch, on until they turn it off
        bool isChecking() const;
        void setChecking (bool shouldCheck);

        // The website's version when it is newer than this build, as last heard. Nothing while switched off
        std::optional<juce::String> availableVersion() const;

        // A day since the website was last asked, by any instance. Reads the file
        bool isDue();

        // Asks on a background thread if due, which reads the file there. For when an editor opens, not when the plugin loads: hosts load plugins to scan them
        void checkIfDue();

        // The same on the calling thread
        void askIfDue();

    private:
        struct State
        {
            bool checking = true;
            juce::int64 lastAskedMs = 0;
            juce::String latestVersion;

            // Known here but not in the file
            bool unsaved = false;
        };

        void run() override;

        State readState() const;
        void readSavedState();
        void writeState();
        bool isDueAsLastRead() const;
        void shareWithOtherInstances();
        std::optional<juce::String> fetchFromWebsite (const juce::String& address);

        const Config config;

        // Under the lock the process's instances share
        State state;

        juce::CriticalSection requestLock;
        juce::WebInputStream* request = nullptr;

        JUCE_DECLARE_NON_COPYABLE (UpdateCheck)
    };
}
