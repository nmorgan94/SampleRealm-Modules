#include "EditorFixtures.h"
#include "LicenceFixtures.h"

using namespace licensing;
using namespace LicenceFixtures;

namespace
{
    // A bar in a parent the editor's size, for the fixtures' machine, with a licence file of its own
    struct BarFixture : EditorFixtures::OverlayFixture, ActivationFixture
    {
        BarFixture()   { parent.addChildComponent (prompt); }

        PromptOverlay prompt;
    };

    juce::StringArray labelTexts (juce::Component& component)
    {
        juce::StringArray texts;

        for (auto* child : component.getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                texts.add (label->getText());

        return texts;
    }
}

//==============================================================================
TEST_CASE_METHOD (BarFixture, "The activation bar shows this machine's code until a valid licence is offered", "[activation][ui]")
{
    ActivationBar bar (activation, prompt);

    CHECK (bar.isVisible());
    CHECK (labelTexts (bar) == juce::StringArray { fixtureMachine().grouped() });

    bar.offer (fixtureText ("valid.licence"));

    CHECK_FALSE (bar.isVisible());
    CHECK_FALSE (prompt.isVisible());
    CHECK (activation.isActivated());

    const ActivationBar onNextOpen (activation, prompt);
    CHECK_FALSE (onNextOpen.isVisible());
}

TEST_CASE_METHOD (BarFixture, "A refused licence leaves the bar up and says why", "[activation][ui]")
{
    ActivationBar bar (activation, prompt);

    bar.offer (fixtureText ("tampered-signature.licence"));

    CHECK (bar.isVisible());
    CHECK (prompt.isVisible());
    CHECK_FALSE (activation.isActivated());
}

TEST_CASE_METHOD (BarFixture, "The bar goes when another instance activates this plugin", "[activation][ui]")
{
    ActivationBar bar (activation, prompt);
    Activation inAnotherPlugin { fixtureMachine(), trusted, temporary.getFile() };

    bar.update();
    CHECK (bar.isVisible());

    REQUIRE (inAnotherPlugin.install (fixtureText ("valid.licence")) == LicenceStatus::valid);
    bar.update();

    CHECK_FALSE (bar.isVisible());
}

TEST_CASE_METHOD (BarFixture, "A machine with no code is not asked to activate", "[activation][ui]")
{
    Activation withoutMachine { std::nullopt, trusted, temporary.getFile() };
    const ActivationBar bar (withoutMachine, prompt);

    CHECK_FALSE (bar.isVisible());
}
