#pragma once

#include "PresetActions.h"
#include "PresetBar.h"

//==============================================================================
/**
 * The strip along the top of the editor: the title, the preset bar, the menu and a meter.
 * Mirrored by web/editor/HeaderBar.tsx. The title's font is the LookAndFeel's EditorFonts.
 */
class HeaderBar : public juce::Component
{
public:
    enum ColourIds
    {
        backgroundColourId = 0x5d10500,
        titleColourId
    };

    /** Each plugin's sizes, as HeaderLayout in web/editor/HeaderBar.tsx. */
    struct Layout
    {
        int height;
        int edge;           // left of the title, right of the meter
        int titleWidth;     // a longer title is squeezed to fit
        float titleHeight;
        int gap;            // either side of the preset bar and menu
        int controlsHeight;
        int meterWidth;
        int meterInset;     // above and below the meter
    };

    /** Bold, then a part in the regular weight, which may be empty. As HeaderTitle in web/editor/HeaderBar.tsx. */
    struct Title
    {
        juce::String bold, regular;
    };

    static constexpr float minimumTitleScale = 0.7f;
    juce::Font titleFont() const;

    /** Both parts, before any squeezing. */
    float titleTextWidth() const;

    /** As drawFittedText: squeezed to fit, as far as minimumTitleScale, then cut short with "…". */
    struct FittedTitle
    {
        Title shown;
        float squeeze;
    };

    static FittedTitle fitTitle (const Title&, const juce::Font& boldFont, const juce::Font& regularFont, int width);

    HeaderBar (const Title&, const Layout&, PresetManager&, PromptOverlay&,
               std::function<void()> showManual, const juce::String& aboutText, juce::Component& meter);

    HeaderBar (const juce::String& boldTitle, const Layout&, PresetManager&, PromptOverlay&,
               std::function<void()> showManual, const juce::String& aboutText, juce::Component& meter);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const Title title;
    const Layout layout;
    PromptOverlay& prompt;
    const std::function<void()> showManual;
    const juce::String aboutText;

    PresetActions presetActions;
    PresetBar presetBar;
    juce::TextButton menuButton { "..." };
    juce::Component& meter;

    juce::Font regularTitleFont() const;
    static float widthOf (const Title&, const juce::Font& boldFont, const juce::Font& regularFont);
    void showMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
