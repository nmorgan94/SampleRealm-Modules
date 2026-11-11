namespace licensing
{
    static std::string_view trimWhitespace (std::string_view text)
    {
        constexpr std::string_view byteOrderMark { "\xEF\xBB\xBF" };
        constexpr std::string_view whitespace { " \t\r\n\v\f" };

        if (text.starts_with (byteOrderMark))
            text.remove_prefix (byteOrderMark.size());

        const auto first = text.find_first_not_of (whitespace);

        if (first == std::string_view::npos)
            return {};

        return text.substr (first, text.find_last_not_of (whitespace) - first + 1);
    }

    static std::optional<LicencePayload> parsePayload (const std::vector<std::uint8_t>& payloadBytes,
                                                       std::string_view verifyingKeyId)
    {
        juce::var parsed;
        const auto json = juce::String::fromUTF8 (reinterpret_cast<const char*> (payloadBytes.data()),
                                                  static_cast<int> (payloadBytes.size()));

        if (juce::JSON::parse (json, parsed).failed() || parsed.getDynamicObject() == nullptr)
            return {};

        const auto version = parsed["v"];
        const auto keyId   = parsed["keyId"];
        const auto machine = parsed["machine"];
        const auto tier    = parsed["tier"];
        const auto issued  = parsed["issued"];

        const auto versionIsNumber = version.isInt() || version.isInt64() || version.isDouble();
        const double versionNumber = version;

        if (! versionIsNumber || versionNumber != 1.0)
            return {};

        if (! keyId.isString() || keyId.toString() != juce::String (verifyingKeyId.data(), verifyingKeyId.size()))
            return {};

        if (! machine.isString() || ! MachineCode::fromCanonical (machine.toString()))
            return {};

        if (! tier.isString() || tier.toString() != "free")
            return {};

        return LicencePayload { 1, keyId.toString(), machine.toString(), tier.toString(),
                                issued.isString() ? issued.toString() : juce::String() };
    }

    LicenceCheck verifyLicence (std::string_view licenceText,
                                const MachineCode& thisMachine,
                                std::span<const TrustedKey> trustedKeys)
    {
        const auto trimmed = trimWhitespace (licenceText);
        const auto separator = trimmed.find ('.');

        if (separator == std::string_view::npos || trimmed.find ('.', separator + 1) != std::string_view::npos)
            return { LicenceStatus::malformed, {} };

        const auto payloadBytes = decodeBase64Url (trimmed.substr (0, separator));
        const auto signature    = decodeBase64Url (trimmed.substr (separator + 1));

        if (! payloadBytes || ! signature || signature->size() != 64)
            return { LicenceStatus::malformed, {} };

        const auto verifyingKey = std::ranges::find_if (trustedKeys, [&] (const TrustedKey& key)
        {
            return crypto_ed25519_check (signature->data(), key.publicKey.data(),
                                         payloadBytes->data(), payloadBytes->size()) == 0;
        });

        if (verifyingKey == trustedKeys.end())
            return { LicenceStatus::signature, {} };

        const auto payload = parsePayload (*payloadBytes, verifyingKey->keyId);

        if (! payload)
            return { LicenceStatus::payload, {} };

        if (payload->machine != thisMachine.canonical())
            return { LicenceStatus::wrongMachine, payload };

        return { LicenceStatus::valid, payload };
    }
}
