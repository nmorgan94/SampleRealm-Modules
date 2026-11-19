namespace updates
{
    static constexpr int connectionTimeoutMs = 4000;

    static juce::String fromConfig (std::string_view text)
    {
        return { text.data(), text.size() };
    }

    // The process's instances share the file, so they take turns with it
    static juce::CriticalSection& stateLock()
    {
        static juce::CriticalSection lock;
        return lock;
    }

    static juce::Array<UpdateCheck*>& instances()
    {
        static juce::Array<UpdateCheck*> all;
        return all;
    }

    static juce::File stateFileLocation()
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif

        return base.getChildFile (fromConfig (config::companyName)).getChildFile (fromConfig (config::productName) + ".updates");
    }

    static std::atomic<bool> updateUrlIgnored { false };

    void UpdateCheck::ignoreUpdateUrl()
    {
        updateUrlIgnored = true;
    }

    UpdateCheck::UpdateCheck()
        : UpdateCheck (Config { .page = updateUrlIgnored ? juce::String() : fromConfig (config::updateUrl),
                                .thisVersion = JucePlugin_VersionString,
                                .stateFile = stateFileLocation() })
    {
    }

    UpdateCheck::UpdateCheck (Config configToUse)
        : juce::Thread ("Update check"), config (std::move (configToUse)), state (readState())
    {
        const juce::ScopedLock lock (stateLock());
        instances().add (this);
    }

    UpdateCheck::~UpdateCheck()
    {
        {
            const juce::ScopedLock lock (stateLock());
            instances().removeFirstMatchingValue (this);
        }

        signalThreadShouldExit();

        {
            const juce::ScopedLock lock (requestLock);

            if (request != nullptr)
                request->cancel();
        }

        // A request that couldn't be cancelled ends at its timeout
        stopThread (-1);
    }

    UpdateCheck::State UpdateCheck::readState() const
    {
        State read;

        if (! config.stateFile.existsAsFile() || std::cmp_greater (config.stateFile.getSize(), largestReplyBytes))
            return read;

        juce::var parsed;

        if (juce::JSON::parse (config.stateFile.loadFileAsString(), parsed).failed() || ! parsed.isObject())
            return read;

        if (parsed["checking"].isBool())
            read.checking = parsed["checking"];

        if (parsed["lastAsked"].isInt() || parsed["lastAsked"].isInt64())
            read.lastAskedMs = parsed["lastAsked"];

        if (parsed["latest"].isString() && isVersion (parsed["latest"].toString()))
            read.latestVersion = parsed["latest"].toString();

        return read;
    }

    void UpdateCheck::readSavedState()
    {
        if (! state.unsaved)
            state = readState();
    }

    // A file that can't be written, as in a sandboxed host, leaves what is known here until the plugin closes
    void UpdateCheck::writeState()
    {
        auto written = std::make_unique<juce::DynamicObject>();
        written->setProperty ("checking", state.checking);
        written->setProperty ("lastAsked", state.lastAskedMs);
        written->setProperty ("latest", state.latestVersion);

        state.unsaved = ! (config.stateFile.getParentDirectory().createDirectory()
                     && config.stateFile.replaceWithText (juce::JSON::toString (juce::var (written.release()), true)));
    }

    // They hear on the message thread
    void UpdateCheck::shareWithOtherInstances()
    {
        for (auto* instance : instances())
        {
            if (instance == this || instance->config.stateFile != config.stateFile)
                continue;

            instance->state = state;
            instance->sendChangeMessage();
        }
    }

    bool UpdateCheck::isChecking() const
    {
        const juce::ScopedLock lock (stateLock());
        return state.checking;
    }

    void UpdateCheck::setChecking (bool shouldCheck)
    {
        {
            const juce::ScopedLock lock (stateLock());

            // Keeps a check another instance made
            readSavedState();
            state.checking = shouldCheck;
            writeState();
            shareWithOtherInstances();
        }

        sendSynchronousChangeMessage();

        if (shouldCheck)
            checkIfDue();
    }

    std::optional<juce::String> UpdateCheck::availableVersion() const
    {
        const juce::ScopedLock lock (stateLock());

        if (! state.checking || ! isNewer (state.latestVersion, config.thisVersion))
            return std::nullopt;

        return state.latestVersion;
    }

    bool UpdateCheck::isDue()
    {
        const juce::ScopedLock lock (stateLock());
        readSavedState();

        return isDueAsLastRead();
    }

    bool UpdateCheck::isDueAsLastRead() const
    {
        const auto sinceAsked = config.now().toMilliseconds() - state.lastAskedMs;

        // A clock put back asks again
        return isConfigured() && state.checking && (sinceAsked < 0 || sinceAsked >= juce::RelativeTime::days (1).inMilliseconds());
    }

    void UpdateCheck::checkIfDue()
    {
        if (isConfigured() && ! isThreadRunning())
            startThread();
    }

    void UpdateCheck::run()
    {
        askIfDue();
    }

    void UpdateCheck::askIfDue()
    {
        {
            const juce::ScopedLock lock (stateLock());
            readSavedState();

            if (! isDueAsLastRead())
                return;

            // Kept whatever the answer, and before asking, so the website is asked once a day at most
            state.lastAskedMs = config.now().toMilliseconds();
            writeState();
        }

        const auto address = versionAddress();
        const auto reply = config.fetch != nullptr ? config.fetch (address) : fetchFromWebsite (address);
        const auto version = reply.has_value() ? versionInReply (*reply) : std::nullopt;

        if (! version.has_value())
            return;

        {
            const juce::ScopedLock lock (stateLock());

            // Keeps a switch turned while asking
            readSavedState();

            if (state.latestVersion == *version)
                return;

            state.latestVersion = *version;
            writeState();
            shareWithOtherInstances();
        }

        sendChangeMessage();
    }

    std::optional<juce::String> UpdateCheck::fetchFromWebsite (const juce::String& address)
    {
        juce::WebInputStream stream (juce::URL (address), false);
        stream.withConnectionTimeout (connectionTimeoutMs);

        // In place of the system's, which name the host and the user's language
        stream.withExtraHeaders ("User-Agent: plugin-update-check\r\nAccept-Language: *");

        {
            const juce::ScopedLock lock (requestLock);

            if (threadShouldExit())
                return std::nullopt;

            request = &stream;
        }

        std::optional<juce::String> reply;

        if (stream.connect (nullptr) && stream.getStatusCode() == 200)
        {
            // One byte over is enough for it to be ignored
            juce::MemoryBlock body;
            stream.readIntoMemoryBlock (body, largestReplyBytes + 1);
            reply = body.toString();
        }

        const juce::ScopedLock lock (requestLock);
        request = nullptr;

        return reply;
    }
}
