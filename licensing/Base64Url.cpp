namespace licensing
{
    static int valueOfBase64UrlCharacter (char character)
    {
        if (character >= 'A' && character <= 'Z')   return character - 'A';
        if (character >= 'a' && character <= 'z')   return character - 'a' + 26;
        if (character >= '0' && character <= '9')   return character - '0' + 52;
        if (character == '-')                       return 62;
        if (character == '_')                       return 63;
        return -1;
    }

    std::optional<std::vector<std::uint8_t>> decodeBase64Url (std::string_view text)
    {
        if (text.size() % 4 == 1)
            return {};

        std::vector<std::uint8_t> bytes;
        bytes.reserve (text.size() * 3 / 4);

        std::uint32_t pendingBits = 0;
        int pendingBitCount = 0;

        for (const auto character : text)
        {
            const auto value = valueOfBase64UrlCharacter (character);

            if (value < 0)
                return {};

            pendingBits = (pendingBits << 6) | static_cast<std::uint32_t> (value);
            pendingBitCount += 6;

            if (pendingBitCount >= 8)
            {
                pendingBitCount -= 8;
                bytes.push_back (static_cast<std::uint8_t> (pendingBits >> pendingBitCount));
            }
        }

        const auto unusedBits = pendingBits & ((1u << pendingBitCount) - 1u);

        if (unusedBits != 0)
            return {};

        return bytes;
    }
}
