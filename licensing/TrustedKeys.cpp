namespace licensing
{
    std::span<const TrustedKey> productionKeys()
    {
        return config::trustedKeys;
    }
}
