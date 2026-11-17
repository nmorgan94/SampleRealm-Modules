UpdateActions::UpdateActions (updates::UpdateCheck& checkToShow)
    : check (checkToShow)
{
    check.addChangeListener (this);
    check.checkIfDue();
}

UpdateActions::~UpdateActions()
{
    check.removeChangeListener (this);
}

juce::String UpdateActions::notice() const
{
    const auto available = check.availableVersion();
    return available.has_value() ? "v" + *available + " available" : juce::String();
}

void UpdateActions::setChecking (bool shouldCheck)
{
    check.setChecking (shouldCheck);
}

void UpdateActions::openPage()
{
    juce::URL (check.pageAddress()).launchInDefaultBrowser();
}

void UpdateActions::addMenuItems (juce::PopupMenu& menu)
{
    if (! isConfigured())
        return;

    menu.addSeparator();

    if (const auto text = notice(); text.isNotEmpty())
        menu.addItem (text, [this] { openPage(); });

    menu.addItem ("Check for updates", true, isChecking(), [this] { setChecking (! isChecking()); });
}

void UpdateActions::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (onChange != nullptr)
        onChange();
}
