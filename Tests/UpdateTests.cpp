#include <updates/updates.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <memory>
#include <vector>

using namespace updates;

namespace
{
    const juce::String page { "https://example.com/plugins/example" };

    // A check for a 1.2.0 build, with a clock and a website of the test's own
    struct UpdateFixture
    {
        UpdateFixture()   { folder.createDirectory(); }
        ~UpdateFixture()  { folder.deleteRecursively(); }

        UpdateCheck::Config config (const juce::String& pageToAsk = page)
        {
            return { .page = pageToAsk,
                     .thisVersion = "1.2.0",
                     .stateFile = stateFile,
                     .fetch = [this] (const juce::String& address)
                     {
                         asked.add (address);
                         return reply;
                     },
                     .now = [this] { return clock; } };
        }

        static juce::String replyWith (const juce::String& version)   { return "{\"version\":\"" + version + "\"}"; }

        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        juce::File folder { juce::File::createTempFile ("updates") };
        juce::File stateFile { folder.getChildFile ("Example").getChildFile ("Example.updates") };

        juce::Time clock { 2026, 10, 17, 9, 0 };
        std::optional<juce::String> reply { replyWith ("1.3.0") };
        juce::StringArray asked;
    };

    juce::StringArray itemTexts (const juce::PopupMenu& menu)
    {
        juce::StringArray texts;

        for (juce::PopupMenu::MenuItemIterator items (menu); items.next();)
            if (! items.getItem().isSeparator)
                texts.add (items.getItem().text);

        return texts;
    }
}

//==============================================================================
TEST_CASE ("A version is up to four dotted numbers", "[updates]")
{
    for (const auto* version : { "0", "1.2", "0.0.1", "1.10.0", "1.2.3.4", "123456789.0" })
        CHECK (isVersion (version));

    for (const auto* notVersion : { "", "v1.2.3", "1.2.3.4.5", "1..2", ".1", "1.", "1.2-beta", "1,2", "1234567890", " 1.2" })
        CHECK_FALSE (isVersion (notVersion));
}

TEST_CASE ("Versions are compared part by part as numbers", "[updates]")
{
    CHECK (isNewer ("0.0.2", "0.0.1"));
    CHECK (isNewer ("1.10.0", "1.9.2"));
    CHECK (isNewer ("2.0", "1.9.9"));
    CHECK (isNewer ("1.2.0.1", "1.2"));

    CHECK_FALSE (isNewer ("0.0.1", "0.0.1"));
    CHECK_FALSE (isNewer ("1.2", "1.2.0"));
    CHECK_FALSE (isNewer ("1.9.2", "1.10.0"));
    CHECK_FALSE (isNewer ("", "0.0.1"));
}

TEST_CASE ("Only the reply the website documents gives a version", "[updates]")
{
    CHECK (versionInReply ("{\"version\":\"0.0.2\"}") == juce::String ("0.0.2"));
    CHECK (versionInReply ("{ \"version\": \"1.2.3\", \"later\": true }\n") == juce::String ("1.2.3"));

    for (const auto* refused : { "", "0.0.2", "\"0.0.2\"", "[\"0.0.2\"]", "{\"version\":2}", "{\"version\":\"v0.0.2\"}",
                                 "{\"version\":\"https://example.com\"}", "{\"error\":\"Plugin not found\"}", "<html>0.0.2</html>" })
        CHECK_FALSE (versionInReply (refused).has_value());

    const auto padding = juce::String::repeatedString (" ", static_cast<int> (largestReplyBytes));
    CHECK_FALSE (versionInReply ("{\"version\":\"0.0.2\"}" + padding).has_value());
}

//==============================================================================
TEST_CASE_METHOD (UpdateFixture, "A newer version on the website is available, and still is when the plugin next opens", "[updates]")
{
    UpdateCheck check (config());

    CHECK_FALSE (check.availableVersion().has_value());

    check.askIfDue();

    CHECK (check.availableVersion() == juce::String ("1.3.0"));
    CHECK (asked == juce::StringArray { page + "/version" });

    const UpdateCheck onNextOpen (config());
    CHECK (onNextOpen.availableVersion() == juce::String ("1.3.0"));
}

TEST_CASE_METHOD (UpdateFixture, "The website's version is no update once this build has it or is past it", "[updates]")
{
    for (const auto* websiteVersion : { "1.2.0", "1.2", "1.1.9", "0.9" })
    {
        stateFile.deleteFile();
        reply = replyWith (websiteVersion);

        UpdateCheck check (config());
        check.askIfDue();

        CHECK (asked.size() == 1);
        CHECK_FALSE (check.availableVersion().has_value());
        asked.clear();
    }
}

TEST_CASE_METHOD (UpdateFixture, "A reply that gives no version changes nothing, and counts as today's ask", "[updates]")
{
    for (const auto& unusable : { std::optional<juce::String>(), std::optional<juce::String> ("not json"), std::optional<juce::String> (replyWith ("v9")) })
    {
        stateFile.deleteFile();
        reply = unusable;

        UpdateCheck check (config());
        check.askIfDue();

        CHECK_FALSE (check.availableVersion().has_value());
        CHECK_FALSE (check.isDue());
    }
}

TEST_CASE_METHOD (UpdateFixture, "A version heard before is kept when a later ask fails", "[updates]")
{
    UpdateCheck check (config());
    check.askIfDue();

    clock += juce::RelativeTime::days (2);
    reply = std::nullopt;
    check.askIfDue();

    CHECK (asked.size() == 2);
    CHECK (check.availableVersion() == juce::String ("1.3.0"));
}

TEST_CASE_METHOD (UpdateFixture, "The website is asked once a day at most, by however many instances", "[updates]")
{
    UpdateCheck check (config());
    UpdateCheck otherInstance (config());

    check.askIfDue();
    check.askIfDue();
    otherInstance.askIfDue();

    clock += juce::RelativeTime::hours (23);
    check.askIfDue();
    otherInstance.askIfDue();

    CHECK (asked.size() == 1);

    clock += juce::RelativeTime::hours (1);
    otherInstance.askIfDue();
    check.askIfDue();

    CHECK (asked.size() == 2);
}

TEST_CASE_METHOD (UpdateFixture, "A clock put back asks again", "[updates]")
{
    UpdateCheck check (config());
    check.askIfDue();

    clock -= juce::RelativeTime::days (30);
    check.askIfDue();

    CHECK (asked.size() == 2);
}

TEST_CASE_METHOD (UpdateFixture, "Switched off, the website isn't asked and a known update isn't shown, until switched back on", "[updates]")
{
    UpdateCheck check (config());
    CHECK (check.isChecking());

    check.askIfDue();
    check.setChecking (false);
    clock += juce::RelativeTime::days (7);
    check.askIfDue();

    CHECK (asked.size() == 1);
    CHECK_FALSE (check.isDue());
    CHECK_FALSE (check.availableVersion().has_value());

    {
        UpdateCheck onNextOpen (config());
        CHECK_FALSE (onNextOpen.isChecking());

        onNextOpen.askIfDue();
        CHECK (asked.size() == 1);
    }

    reply = std::nullopt;
    check.setChecking (true);
    CHECK (check.availableVersion() == juce::String ("1.3.0"));
}

TEST_CASE_METHOD (UpdateFixture, "Switching off keeps the ask another instance made today", "[updates]")
{
    UpdateCheck check (config());
    UpdateCheck otherInstance (config());

    otherInstance.askIfDue();
    check.setChecking (false);
    check.setChecking (true);
    check.askIfDue();

    CHECK (asked.size() == 1);
    CHECK (check.availableVersion() == juce::String ("1.3.0"));
}

TEST_CASE_METHOD (UpdateFixture, "A switch turned in another process while the website is asked survives the reply", "[updates]")
{
    auto switchingConfig = config();
    switchingConfig.fetch = [&] (const juce::String&)
    {
        stateFile.replaceWithText ("{\"checking\": false, \"lastAsked\": " + juce::String (clock.toMilliseconds()) + ", \"latest\": \"\"}");
        return reply;
    };

    UpdateCheck check (switchingConfig);
    check.askIfDue();

    CHECK_FALSE (check.isChecking());
    CHECK_FALSE (UpdateCheck (config()).isChecking());

    check.setChecking (true);
    CHECK (check.availableVersion() == juce::String ("1.3.0"));
}

TEST_CASE_METHOD (UpdateFixture, "Where the file can't be written, the switch and today's ask still hold until the plugin closes", "[updates]")
{
    // A file where the folder would be
    stateFile.getParentDirectory().create();

    UpdateCheck check (config());
    check.askIfDue();
    check.askIfDue();

    CHECK (asked.size() == 1);
    CHECK (check.availableVersion() == juce::String ("1.3.0"));
    REQUIRE_FALSE (stateFile.existsAsFile());

    check.setChecking (false);
    clock += juce::RelativeTime::days (7);
    check.askIfDue();

    CHECK_FALSE (check.isChecking());
    CHECK (asked.size() == 1);
}

TEST_CASE_METHOD (UpdateFixture, "Instances whose editors open together ask the website once between them", "[updates]")
{
    std::atomic<int> timesAsked { 0 };

    auto countingConfig = config();
    countingConfig.fetch = [&] (const juce::String&)
    {
        ++timesAsked;
        juce::Thread::sleep (20);
        return std::optional<juce::String> (replyWith ("1.3.0"));
    };

    {
        std::vector<std::unique_ptr<UpdateCheck>> restoredSession;

        for (int instance = 0; instance < 16; ++instance)
            restoredSession.push_back (std::make_unique<UpdateCheck> (countingConfig));

        for (auto& instance : restoredSession)
            instance->checkIfDue();

        juce::Thread::sleep (200);
    }

    CHECK (timesAsked == 1);
}

TEST_CASE_METHOD (UpdateFixture, "Another open instance has the update found and the switch turned, even where the file can't be written", "[updates]")
{
    for (const auto fileCanBeWritten : { true, false })
    {
        folder.deleteRecursively();
        folder.createDirectory();

        if (! fileCanBeWritten)
            stateFile.getParentDirectory().create();

        UpdateCheck check (config());
        UpdateCheck otherInstance (config());

        check.askIfDue();
        CHECK (otherInstance.availableVersion() == juce::String ("1.3.0"));

        check.setChecking (false);
        CHECK_FALSE (otherInstance.isChecking());

        clock += juce::RelativeTime::days (7);
        otherInstance.askIfDue();
        CHECK (asked.size() == 1);

        asked.clear();
    }
}

TEST_CASE_METHOD (UpdateFixture, "A build with no page never asks", "[updates]")
{
    UpdateCheck check (config ({}));

    CHECK_FALSE (check.isConfigured());
    CHECK_FALSE (check.isDue());

    check.askIfDue();
    check.checkIfDue();

    CHECK (asked.isEmpty());
    CHECK_FALSE (stateFile.existsAsFile());
}

TEST_CASE_METHOD (UpdateFixture, "A file that isn't the check's own is read as a first run", "[updates]")
{
    stateFile.getParentDirectory().createDirectory();

    for (const auto* contents : { "", "not json", "[]", "{\"checking\":\"no\",\"lastAsked\":\"today\",\"latest\":\"https://example.com\"}" })
    {
        stateFile.replaceWithText (contents);
        UpdateCheck check (config());

        CHECK (check.isChecking());
        CHECK (check.isDue());
        CHECK_FALSE (check.availableVersion().has_value());
    }

    stateFile.replaceWithText (juce::String::repeatedString (" ", static_cast<int> (largestReplyBytes)) + "{\"checking\":false}");
    CHECK (UpdateCheck (config()).isChecking());
}

TEST_CASE_METHOD (UpdateFixture, "A due check runs on a background thread, and closing the plugin waits for it", "[updates]")
{
    juce::WaitableEvent fetchStarted;
    std::atomic<bool> fetchFinished { false };

    auto slowConfig = config();
    slowConfig.fetch = [&] (const juce::String&)
    {
        fetchStarted.signal();
        juce::Thread::sleep (100);
        fetchFinished = true;
        return std::optional<juce::String> (replyWith ("1.3.0"));
    };

    {
        UpdateCheck check (slowConfig);
        check.checkIfDue();
        check.checkIfDue();

        REQUIRE (fetchStarted.wait (5000));
        CHECK_FALSE (fetchFinished);
    }

    CHECK (fetchFinished);
    CHECK (UpdateCheck (config()).availableVersion() == juce::String ("1.3.0"));
}

//==============================================================================
namespace
{
    // The switch is the last item
    bool switchIsTicked (const juce::PopupMenu& menu)
    {
        auto ticked = false;

        for (juce::PopupMenu::MenuItemIterator items (menu); items.next();)
            ticked = items.getItem().isTicked;

        return ticked;
    }
}

TEST_CASE_METHOD (UpdateFixture, "The menu has the notice only while there is an update, and always the switch", "[updates][ui]")
{
    reply = replyWith ("1.2.0");

    UpdateCheck check (config());
    check.askIfDue();

    UpdateActions actions (check);

    juce::PopupMenu upToDate;
    actions.addMenuItems (upToDate);
    CHECK (itemTexts (upToDate) == juce::StringArray { "Check for updates" });
    CHECK (switchIsTicked (upToDate));
    CHECK (actions.notice().isEmpty());

    clock += juce::RelativeTime::days (1);
    reply = replyWith ("1.3.0");
    check.askIfDue();

    juce::PopupMenu withUpdate;
    actions.addMenuItems (withUpdate);
    CHECK (itemTexts (withUpdate) == juce::StringArray { "v1.3.0 available", "Check for updates" });
    CHECK (switchIsTicked (withUpdate));
    CHECK (actions.notice() == "v1.3.0 available");
}

TEST_CASE_METHOD (UpdateFixture, "The items come under a separator in a menu that has others", "[updates][ui]")
{
    UpdateCheck check (config());
    check.askIfDue();

    UpdateActions actions (check);

    juce::PopupMenu menu;
    menu.addItem ("About", [] {});
    actions.addMenuItems (menu);

    juce::PopupMenu::MenuItemIterator items (menu);
    items.next();
    items.next();
    CHECK (items.getItem().isSeparator);
    CHECK (itemTexts (menu) == juce::StringArray { "About", "v1.3.0 available", "Check for updates" });
}

TEST_CASE_METHOD (UpdateFixture, "Turning the switch off unticks it, hides the notice and tells the editor", "[updates][ui]")
{
    UpdateCheck check (config());
    check.askIfDue();

    UpdateActions actions (check);
    int changes = 0;
    actions.onChange = [&changes] { ++changes; };

    actions.setChecking (false);

    juce::PopupMenu menu;
    actions.addMenuItems (menu);
    CHECK (itemTexts (menu) == juce::StringArray { "Check for updates" });
    CHECK_FALSE (switchIsTicked (menu));
    CHECK (changes == 1);
}

TEST_CASE_METHOD (UpdateFixture, "A build with no page adds nothing to the menu", "[updates][ui]")
{
    UpdateCheck check (config ({}));
    UpdateActions actions (check);

    juce::PopupMenu menu;
    actions.addMenuItems (menu);

    CHECK (menu.getNumItems() == 0);
}

TEST_CASE_METHOD (UpdateFixture, "Opening an editor starts the check that is due", "[updates][ui]")
{
    juce::WaitableEvent fetched;

    auto signallingConfig = config();
    signallingConfig.fetch = [&] (const juce::String&)
    {
        fetched.signal();
        return std::optional<juce::String>();
    };

    UpdateCheck check (signallingConfig);
    const UpdateActions actions (check);

    CHECK (fetched.wait (5000));
}
