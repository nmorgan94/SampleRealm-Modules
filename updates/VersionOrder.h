namespace updates
{
    // Nothing longer is read as a reply
    inline constexpr std::size_t largestReplyBytes = 1024;

    // Dotted numbers, as 1.2.3: up to four parts of up to nine digits
    bool isVersion (const juce::String& text);

    // Each part as a number, a missing one as 0: 1.10.0 is newer than 1.9.2, and 1.2 is no newer than 1.2.0
    bool isNewer (const juce::String& version, const juce::String& thanVersion);

    // The version in the website's reply, as its docs/update-check.md. Nothing for any other reply
    std::optional<juce::String> versionInReply (const juce::String& reply);
}
