namespace licensing
{
    static constexpr std::size_t largestRequestLine = 4096;

    // A browser's cookies for 127.0.0.1 come along, so the headers get more room
    static constexpr std::size_t largestRequest = 65536;

    static constexpr int requestTimeoutMs = 2000;
    static constexpr int pollMs = 100;

    static void respond (juce::StreamingSocket& browser, const juce::String& statusLine, const juce::String& location = {})
    {
        auto response = "HTTP/1.1 " + statusLine + "\r\n";

        if (location.isNotEmpty())
            response << "Location: " << location << "\r\n";

        response << "Content-Length: 0\r\nConnection: close\r\n\r\n";
        browser.write (response.toRawUTF8(), static_cast<int> (response.getNumBytesAsUTF8()));
    }

    //==============================================================================
    LoopbackReceiver::LoopbackReceiver (Activation& activationToInstallInto)
        : Thread ("Licence receiver"), activation (activationToInstallInto)
    {
    }

    LoopbackReceiver::~LoopbackReceiver()
    {
        stop();
    }

    juce::String LoopbackReceiver::newState()
    {
        return juce::Uuid().toString();
    }

    std::optional<int> LoopbackReceiver::start (const juce::String& stateForThisAttempt)
    {
        stop();

        // Port 0 lets the system pick a free one
        if (stateForThisAttempt.isEmpty() || ! listener.createListener (0, "127.0.0.1"))
            return {};

        state = stateForThisAttempt;
        const auto port = listener.getBoundPort();

        if (! startThread())
        {
            listener.close();
            return {};
        }

        return port;
    }

    void LoopbackReceiver::stop()
    {
        // Waits, as a thread killed while installing would keep the licence file locked
        stopThread (-1);
        listener.close();
    }

    void LoopbackReceiver::run()
    {
        const auto giveUpAt = juce::Time::getCurrentTime() + juce::RelativeTime::minutes (10);

        while (! threadShouldExit() && juce::Time::getCurrentTime() < giveUpAt)
        {
            const auto waiting = listener.waitUntilReady (true, pollMs);

            // A listener that failed answers at once, every time
            if (waiting < 0)
                break;

            if (waiting == 0)
                continue;

            const std::unique_ptr<juce::StreamingSocket> browser (listener.waitForNextConnection());

            if (browser != nullptr && answer (*browser))
                break;
        }

        // So a late request is refused at once, not left waiting
        listener.close();
    }

    juce::String LoopbackReceiver::readRequest (juce::StreamingSocket& browser) const
    {
        std::string request;
        const auto deadline = juce::Time::getCurrentTime() + juce::RelativeTime::milliseconds (requestTimeoutMs);

        for (;;)
        {
            const auto requestLineLength = std::min (request.find ("\r\n"), request.size());

            if (requestLineLength > largestRequestLine || request.size() > largestRequest)
                return {};

            if (request.find ("\r\n\r\n") != std::string::npos)
                return juce::String::fromUTF8 (request.data(), static_cast<int> (request.size()));

            if (threadShouldExit() || juce::Time::getCurrentTime() > deadline)
                return {};

            const auto waiting = browser.waitUntilReady (true, pollMs);

            if (waiting < 0)
                return {};

            if (waiting == 0)
                continue;

            char received[512];
            const auto count = browser.read (received, sizeof (received), false);

            if (count <= 0)
                return {};

            request.append (received, static_cast<std::size_t> (count));
        }
    }

    bool LoopbackReceiver::answer (juce::StreamingSocket& browser)
    {
        // "GET /licence?licence=<licence>&state=<state> HTTP/1.1"
        const auto requestLine = juce::StringArray::fromTokens (readRequest (browser).upToFirstOccurrenceOf ("\r\n", false, false), " ", "");

        if (requestLine.size() != 3 || requestLine[0] != "GET" || ! requestLine[1].startsWith ("/licence?"))
        {
            respond (browser, "404 Not Found");
            return false;
        }

        juce::StringPairArray query;

        for (const auto& parameter : juce::StringArray::fromTokens (requestLine[1].fromFirstOccurrenceOf ("?", false, false), "&", ""))
            query.set (parameter.upToFirstOccurrenceOf ("=", false, false), parameter.fromFirstOccurrenceOf ("=", false, false));

        if (query["state"] != state)
        {
            respond (browser, "403 Forbidden");
            return false;
        }

        const auto installed = activation.install (query["licence"].toStdString());
        respond (browser, "302 Found", Activation::finishedAddress (installed));
        return true;
    }
}
