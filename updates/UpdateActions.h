//==============================================================================
/**
 * What the menu's update items show and do, for the native menu and the WebView's.
 * Making one starts a check if one is due, so an editor makes it as it opens.
 */
class UpdateActions : private juce::ChangeListener
{
public:
    explicit UpdateActions (updates::UpdateCheck&);
    ~UpdateActions() override;

    // False in a build with no UPDATE_URL, which shows no items
    bool isConfigured() const    { return check.isConfigured(); }

    // "v0.0.2 available", or empty when there is nothing newer
    juce::String notice() const;

    bool isChecking() const      { return check.isChecking(); }
    void setChecking (bool shouldCheck);

    // The plugin's page, in the browser
    void openPage();

    // The notice when there is one, and the switch
    void addMenuItems (juce::PopupMenu&);

    // The notice or the switch changed
    std::function<void()> onChange;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    updates::UpdateCheck& check;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateActions)
};
