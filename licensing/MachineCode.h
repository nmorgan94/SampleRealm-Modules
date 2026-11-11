namespace licensing
{
    /** This machine's code: 16 Crockford base32 characters, uppercase, no hyphens. */
    class MachineCode
    {
    public:
        static constexpr int length = 16;

        // The company's salt, permanent once a plugin ships: "<COMPANY_NAME>/machine-code/v1"
        static std::string companySalt();

        // nullopt when the system gives no device ID
        static std::optional<MachineCode> forThisMachine();

        // The recipe: nullopt for an empty ID, which would otherwise be every failing machine's code
        static std::optional<MachineCode> fromDeviceId (const juce::String& deviceId, std::string_view salt);

        // nullopt unless the text is already canonical
        static std::optional<MachineCode> fromCanonical (const juce::String& text);

        const juce::String& canonical() const   { return code; }

        // K7Q2-M9XH-4TBV-8RNC
        juce::String grouped() const;

        bool operator== (const MachineCode&) const = default;

    private:
        explicit MachineCode (juce::String canonicalCode) : code (std::move (canonicalCode)) {}

        juce::String code;
    };
}
