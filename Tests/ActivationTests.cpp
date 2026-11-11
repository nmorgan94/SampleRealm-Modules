#include "LicenceFixtures.h"

using namespace licensing;
using namespace LicenceFixtures;

namespace
{
    // What the receiver answers, or empty if it only hangs up
    juce::String sendToReceiver (int port, const juce::String& request, const juce::String& host = "127.0.0.1")
    {
        juce::StreamingSocket browser;

        if (! browser.connect (host, port, 2000))
            return "could not connect";

        browser.write (request.toRawUTF8(), static_cast<int> (request.getNumBytesAsUTF8()));

        std::string response;

        while (response.find ("\r\n\r\n") == std::string::npos && browser.waitUntilReady (true, 5000) == 1)
        {
            char received[512];
            const auto count = browser.read (received, sizeof (received), false);

            if (count <= 0)
                break;

            response.append (received, static_cast<std::size_t> (count));
        }

        return juce::String::fromUTF8 (response.data(), static_cast<int> (response.size()));
    }

    juce::String licenceRequest (const std::string& licence, const juce::String& state, const juce::String& headers = "Host: 127.0.0.1\r\n")
    {
        return "GET /licence?licence=" + juce::String (licence).trim() + "&state=" + state + " HTTP/1.1\r\n" + headers + "\r\n";
    }

    bool stopsListening (const LoopbackReceiver& receiver)
    {
        for (int attempt = 0; attempt < 100 && receiver.isListening(); ++attempt)
            juce::Thread::sleep (20);

        return ! receiver.isListening();
    }
}

//==============================================================================
TEST_CASE_METHOD (ActivationFixture, "Installing a valid licence keeps it in the licence file", "[activation]")
{
    REQUIRE (activation.status() == LicenceStatus::noLicenceFile);

    CHECK (activation.install (" \r\n" + fixtureText ("valid.licence") + "\n") == LicenceStatus::valid);
    CHECK (activation.isActivated());
    CHECK (licenceFile.loadFileAsString() == juce::String (fixtureText ("valid.licence")).trim());

    const Activation onNextLaunch { fixtureMachine(), trusted, licenceFile };
    CHECK (onNextLaunch.isActivated());
}

TEST_CASE_METHOD (ActivationFixture, "A licence that isn't valid for this machine is refused and nothing is written", "[activation]")
{
    CHECK (activation.install (fixtureText ("tampered-signature.licence")) == LicenceStatus::signature);
    CHECK (activation.install ("not a licence") == LicenceStatus::malformed);
    CHECK (activation.install (fixtureText ("valid.licence") + std::string (largestLicenceBytes, '\n')) == LicenceStatus::malformed);
    CHECK_FALSE (activation.isActivated());
    CHECK_FALSE (licenceFile.exists());

    const auto anotherMachine = MachineCode::fromDeviceId ("another machine", "Example/machine-code/v1");
    const juce::TemporaryFile anotherTemporary (".licence");
    Activation onAnotherMachine { anotherMachine, trusted, anotherTemporary.getFile() };

    CHECK (onAnotherMachine.install (fixtureText ("valid.licence")) == LicenceStatus::wrongMachine);
    CHECK_FALSE (anotherTemporary.getFile().exists());
}

TEST_CASE_METHOD (ActivationFixture, "A refused licence leaves an installed one in place", "[activation]")
{
    REQUIRE (activation.install (fixtureText ("valid.licence")) == LicenceStatus::valid);
    const auto installed = licenceFile.loadFileAsString();

    CHECK (activation.install (fixtureText ("tampered-payload.licence")) == LicenceStatus::signature);
    CHECK (activation.isActivated());
    CHECK (licenceFile.loadFileAsString() == installed);
}

TEST_CASE_METHOD (ActivationFixture, "A machine with no code can't be activated", "[activation]")
{
    const juce::TemporaryFile anotherTemporary (".licence");
    Activation withoutMachine { std::nullopt, trusted, anotherTemporary.getFile() };

    CHECK (withoutMachine.status() == LicenceStatus::noMachineCode);
    CHECK (withoutMachine.install (fixtureText ("valid.licence")) == LicenceStatus::noMachineCode);
    CHECK_FALSE (anotherTemporary.getFile().exists());
    CHECK (withoutMachine.beginInBrowser() == withoutMachine.pageAddress());
}

TEST_CASE_METHOD (ActivationFixture, "The page's address carries the machine code, and the port and state when listening", "[activation]")
{
    const auto page = activation.pageAddress();

    CHECK (page.endsWith ("?machine=" + fixtureMachine().canonical()));
    CHECK (activation.pageAddress (54321, "abc123") == page + "&port=54321&state=abc123");
}

//==============================================================================
TEST_CASE_METHOD (ActivationFixture, "The receiver installs a licence sent with its state, sends the browser on and stops", "[activation][receiver]")
{
    LoopbackReceiver receiver (activation);
    const auto state = LoopbackReceiver::newState();
    const auto port = receiver.start (state);
    REQUIRE (port);

    const auto response = sendToReceiver (*port, licenceRequest (fixtureText ("valid.licence"), state));

    CHECK (response.startsWith ("HTTP/1.1 302 "));
    CHECK (response.contains ("Location: " + Activation::finishedAddress (LicenceStatus::valid) + "\r\n"));
    CHECK (activation.isActivated());
    CHECK (licenceFile.existsAsFile());
    CHECK (stopsListening (receiver));
    CHECK (sendToReceiver (*port, licenceRequest (fixtureText ("valid.licence"), state)) == "could not connect");
}

TEST_CASE_METHOD (ActivationFixture, "The receiver takes a request with long headers, as a browser's cookies make it", "[activation][receiver]")
{
    LoopbackReceiver receiver (activation);
    const auto port = receiver.start ("state");
    REQUIRE (port);

    const auto withCookieOf = [] (int length)
    {
        return licenceRequest (fixtureText ("valid.licence"), "state", "Cookie: " + juce::String::repeatedString ("a", length) + "\r\n");
    };

    sendToReceiver (*port, withCookieOf (70000));
    CHECK_FALSE (activation.isActivated());

    const auto response = sendToReceiver (*port, withCookieOf (6000));

    CHECK (response.startsWith ("HTTP/1.1 302 "));
    CHECK (activation.isActivated());
}

TEST_CASE_METHOD (ActivationFixture, "Stopping the receiver doesn't wait for a connection that sends nothing", "[activation][receiver]")
{
    LoopbackReceiver receiver (activation);
    const auto port = receiver.start ("state");
    REQUIRE (port);

    juce::StreamingSocket silent;
    REQUIRE (silent.connect ("127.0.0.1", *port, 2000));
    juce::Thread::sleep (50);

    const auto stopStarted = juce::Time::getMillisecondCounter();
    receiver.stop();

    CHECK (juce::Time::getMillisecondCounter() - stopStarted < 1000);
    CHECK_FALSE (receiver.isListening());
}

TEST_CASE_METHOD (ActivationFixture, "The receiver gives up on a connection that sends nothing and takes the next", "[activation][receiver]")
{
    LoopbackReceiver receiver (activation);
    const auto port = receiver.start ("state");
    REQUIRE (port);

    juce::StreamingSocket silent;
    REQUIRE (silent.connect ("127.0.0.1", *port, 2000));

    const auto response = sendToReceiver (*port, licenceRequest (fixtureText ("valid.licence"), "state"));

    CHECK (response.startsWith ("HTTP/1.1 302"));
    CHECK (activation.isActivated());
}

TEST_CASE_METHOD (ActivationFixture, "The receiver ignores a request without its state and keeps listening", "[activation][receiver]")
{
    LoopbackReceiver receiver (activation);
    const auto port = receiver.start ("the-right-state");
    REQUIRE (port);

    const auto valid = fixtureText ("valid.licence");

    CHECK (sendToReceiver (*port, licenceRequest (valid, "another-state")).startsWith ("HTTP/1.1 403 "));
    CHECK (sendToReceiver (*port, "GET /licence?licence=" + juce::String (valid).trim() + " HTTP/1.1\r\n\r\n").startsWith ("HTTP/1.1 403 "));
    CHECK (sendToReceiver (*port, "GET /favicon.ico HTTP/1.1\r\n\r\n").startsWith ("HTTP/1.1 404 "));
    CHECK (sendToReceiver (*port, "POST /licence?licence=" + juce::String (valid).trim() + "&state=the-right-state HTTP/1.1\r\n\r\n").startsWith ("HTTP/1.1 404 "));

    // Right in every way but its length
    sendToReceiver (*port, "GET /licence?licence=" + juce::String (valid).trim() + "&state=the-right-state&padding="
                               + juce::String::repeatedString ("a", 5000) + " HTTP/1.1\r\n\r\n");

    CHECK_FALSE (activation.isActivated());
    CHECK_FALSE (licenceFile.exists());
    CHECK (receiver.isListening());

    CHECK (sendToReceiver (*port, licenceRequest (valid, "the-right-state")).startsWith ("HTTP/1.1 302 "));
    CHECK (activation.isActivated());
}

TEST_CASE_METHOD (ActivationFixture, "The receiver tells the browser when the licence was refused", "[activation][receiver]")
{
    LoopbackReceiver receiver (activation);
    const auto port = receiver.start ("state");
    REQUIRE (port);

    const auto response = sendToReceiver (*port, licenceRequest (fixtureText ("tampered-signature.licence"), "state"));

    CHECK (response.contains ("Location: " + Activation::finishedAddress (LicenceStatus::signature) + "\r\n"));
    CHECK (Activation::finishedAddress (LicenceStatus::signature) != Activation::finishedAddress (LicenceStatus::valid));
    CHECK_FALSE (activation.isActivated());
    CHECK (stopsListening (receiver));
}

TEST_CASE_METHOD (ActivationFixture, "Beginning in the browser gives an address whose port and state the plugin answers to", "[activation][receiver]")
{
    const auto address = juce::URL (activation.beginInBrowser());
    const auto names = address.getParameterNames();
    const auto values = address.getParameterValues();

    REQUIRE (names == juce::StringArray { "machine", "port", "state" });
    CHECK (values[0] == fixtureMachine().canonical());
    CHECK (values[2].length() >= 16);

    const auto response = sendToReceiver (values[1].getIntValue(), licenceRequest (fixtureText ("valid.licence"), values[2]));

    CHECK (response.startsWith ("HTTP/1.1 302 "));
    CHECK (activation.isActivated());

    // Each attempt has a state of its own
    CHECK (juce::URL (activation.beginInBrowser()).getParameterValues()[2] != values[2]);
}

TEST_CASE_METHOD (ActivationFixture, "The receiver listens on this machine only", "[activation][receiver]")
{
    LoopbackReceiver receiver (activation);
    const auto port = receiver.start ("state");
    REQUIRE (port);

    const auto networkAddress = juce::IPAddress::getLocalAddress();

    if (networkAddress == juce::IPAddress::local() || networkAddress.isNull())
        SKIP ("This machine has no network address to try");

    CHECK (sendToReceiver (*port, "GET /favicon.ico HTTP/1.1\r\n\r\n", networkAddress.toString()) == "could not connect");
    CHECK (sendToReceiver (*port, "GET /favicon.ico HTTP/1.1\r\n\r\n").startsWith ("HTTP/1.1 404 "));
}
