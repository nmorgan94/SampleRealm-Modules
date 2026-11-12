namespace licensing
{
    static constexpr std::string_view crockfordAlphabet { "0123456789ABCDEFGHJKMNPQRSTVWXYZ" };

    static const std::uint8_t* bytesOf (const char* text)
    {
        return reinterpret_cast<const std::uint8_t*> (text);
    }

    std::string MachineCode::productSalt()
    {
        return std::string (config::companyName) + "/" + std::string (config::productName) + "/machine-code/v1";
    }

    std::optional<MachineCode> MachineCode::forThisMachine()
    {
        return fromDeviceId (juce::SystemStats::getUniqueDeviceID(), productSalt());
    }

    std::optional<MachineCode> MachineCode::fromDeviceId (const juce::String& deviceId, std::string_view salt)
    {
        if (deviceId.isEmpty())
            return {};

        std::array<std::uint8_t, 64> hash;

        crypto_sha512_hmac (hash.data(),
                            bytesOf (salt.data()), salt.size(),
                            bytesOf (deviceId.toRawUTF8()), deviceId.getNumBytesAsUTF8());

        // 80 bits, five to a character
        juce::String code;
        std::uint32_t pendingBits = 0;
        int pendingBitCount = 0;

        for (std::size_t byteIndex = 0; byteIndex < 10; ++byteIndex)
        {
            pendingBits = (pendingBits << 8) | hash[byteIndex];
            pendingBitCount += 8;

            while (pendingBitCount >= 5)
            {
                pendingBitCount -= 5;
                code << crockfordAlphabet[(pendingBits >> pendingBitCount) & 31u];
            }
        }

        return MachineCode { code };
    }

    std::optional<MachineCode> MachineCode::fromCanonical (const juce::String& text)
    {
        if (text.length() != length || ! text.containsOnly (crockfordAlphabet.data()))
            return {};

        return MachineCode { text };
    }

    juce::String MachineCode::grouped() const
    {
        juce::StringArray groups;

        for (int start = 0; start < length; start += 4)
            groups.add (code.substring (start, start + 4));

        return groups.joinIntoString ("-");
    }
}
