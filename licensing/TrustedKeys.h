namespace licensing
{
    using PublicKey = std::array<std::uint8_t, 32>;

    struct TrustedKey
    {
        std::string_view keyId;
        PublicKey publicKey;
    };

    // 64 hex characters, as the website prints a key
    constexpr std::optional<PublicKey> publicKeyFromHex (std::string_view hex)
    {
        constexpr auto valueOfHexDigit = [] (char digit) -> int
        {
            if (digit >= '0' && digit <= '9')   return digit - '0';
            if (digit >= 'a' && digit <= 'f')   return digit - 'a' + 10;
            if (digit >= 'A' && digit <= 'F')   return digit - 'A' + 10;
            return -1;
        };

        PublicKey key {};

        if (hex.size() != key.size() * 2)
            return {};

        for (std::size_t byteIndex = 0; byteIndex < key.size(); ++byteIndex)
        {
            const auto high = valueOfHexDigit (hex[byteIndex * 2]);
            const auto low  = valueOfHexDigit (hex[byteIndex * 2 + 1]);

            if (high < 0 || low < 0)
                return {};

            key[byteIndex] = static_cast<std::uint8_t> (high * 16 + low);
        }

        return key;
    }

    // The only keys a plugin trusts: LICENCE_KEYS when it was configured
    std::span<const TrustedKey> productionKeys();
}
