namespace licensing
{
    // RFC 4648 §5 without padding, strict
    std::optional<std::vector<std::uint8_t>> decodeBase64Url (std::string_view text);
}
