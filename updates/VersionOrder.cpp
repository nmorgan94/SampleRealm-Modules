namespace updates
{
    static juce::StringArray partsOf (const juce::String& version)
    {
        return juce::StringArray::fromTokens (version, ".", {});
    }

    bool isVersion (const juce::String& text)
    {
        const auto parts = partsOf (text);

        if (parts.isEmpty() || parts.size() > 4)
            return false;

        for (const auto& part : parts)
            if (part.isEmpty() || part.length() > 9 || ! part.containsOnly ("0123456789"))
                return false;

        return true;
    }

    bool isNewer (const juce::String& version, const juce::String& thanVersion)
    {
        const auto parts = partsOf (version);
        const auto otherParts = partsOf (thanVersion);

        for (int index = 0; index < juce::jmax (parts.size(), otherParts.size()); ++index)
        {
            const auto part = parts[index].getIntValue();
            const auto otherPart = otherParts[index].getIntValue();

            if (part != otherPart)
                return part > otherPart;
        }

        return false;
    }

    std::optional<juce::String> versionInReply (const juce::String& reply)
    {
        if (reply.getNumBytesAsUTF8() > largestReplyBytes)
            return std::nullopt;

        juce::var parsed;

        if (juce::JSON::parse (reply, parsed).failed() || ! parsed.isObject())
            return std::nullopt;

        const auto version = parsed["version"];

        if (! version.isString() || ! isVersion (version.toString()))
            return std::nullopt;

        return version.toString();
    }
}
