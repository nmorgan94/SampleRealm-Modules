namespace licensing
{
    /**
     * Takes the licence the website's page sends the browser back with, on 127.0.0.1 only.
     * One attempt: it stops after a request carrying its state, or after ten minutes.
     */
    class LoopbackReceiver : private juce::Thread
    {
    public:
        explicit LoopbackReceiver (Activation&);
        ~LoopbackReceiver() override;

        // The port to give the page, or nullopt where the host allows no listener
        std::optional<int> start (const juce::String& stateForThisAttempt);
        void stop();

        bool isListening() const   { return isThreadRunning(); }

        static juce::String newState();

    private:
        void run() override;

        // Up to the blank line that ends the headers, or empty
        juce::String readRequest (juce::StreamingSocket& browser) const;

        // True once a request carried the state
        bool answer (juce::StreamingSocket& browser);

        Activation& activation;
        juce::StreamingSocket listener;
        juce::String state;
    };
}
