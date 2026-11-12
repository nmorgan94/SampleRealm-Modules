#include "HeaderBar.h"

HeaderBar::HeaderBar (const Title& t, const Layout& l, PresetManager& presets, PromptOverlay& o,
                      std::function<void()> m, const juce::String& about, juce::Component& meterToShow)
    : title (t), layout (l), prompt (o), showManual (std::move (m)), aboutText (about),
      presetActions (presets, prompt), presetBar (presets, presetActions), meter (meterToShow)
{
    // For screen readers, as the title is painted
    setTitle (title.bold + title.regular);

    menuButton.onClick = [this] { showMenu(); };

    addAndMakeVisible (presetBar);
    addAndMakeVisible (menuButton);
    addAndMakeVisible (meter);
}

HeaderBar::HeaderBar (const juce::String& boldTitle, const Layout& l, PresetManager& presets, PromptOverlay& o,
                      std::function<void()> m, const juce::String& about, juce::Component& meterToShow)
    : HeaderBar (Title { boldTitle, {} }, l, presets, o, std::move (m), about, meterToShow)
{
}

juce::Font HeaderBar::titleFont() const
{
    return EditorFonts::of (*this, layout.titleHeight, true);
}

juce::Font HeaderBar::regularTitleFont() const
{
    return EditorFonts::of (*this, layout.titleHeight, false);
}

float HeaderBar::widthOf (const Title& text, const juce::Font& boldFont, const juce::Font& regularFont)
{
    return juce::GlyphArrangement::getStringWidth (boldFont, text.bold) + juce::GlyphArrangement::getStringWidth (regularFont, text.regular);
}

float HeaderBar::titleTextWidth() const
{
    return widthOf (title, titleFont(), regularTitleFont());
}

HeaderBar::FittedTitle HeaderBar::fitTitle (const Title& title, const juce::Font& boldFont, const juce::Font& regularFont, int width)
{
    const auto room = static_cast<float> (width) / minimumTitleScale;
    auto shown = title;

    if (widthOf (shown, boldFont, regularFont) > room)
    {
        const juce::String ellipsis = juce::String::fromUTF8 ("\xe2\x80\xa6");

        while ((shown.regular.isNotEmpty() || shown.bold.isNotEmpty())
               && widthOf ({ shown.bold, shown.regular + ellipsis }, boldFont, regularFont) > room)
        {
            if (shown.regular.isNotEmpty())
                shown.regular = shown.regular.dropLastCharacters (1);
            else
                shown.bold = shown.bold.dropLastCharacters (1);
        }

        shown.regular += ellipsis;
    }

    return { shown, std::clamp (static_cast<float> (width) / widthOf (shown, boldFont, regularFont), minimumTitleScale, 1.0f) };
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.fillAll (findColour (backgroundColourId));

    // The meter's repaints reach here too
    const auto titleArea = getLocalBounds().withTrimmedLeft (layout.edge).withWidth (layout.titleWidth);
    if (! g.clipRegionIntersects (titleArea))
        return;

    // Painted, so the colour comes from the LookAndFeel
    const auto colour = findColour (titleColourId);
    const auto boldFont = titleFont();

    if (title.regular.isEmpty())
    {
        g.setColour (colour);
        g.setFont (boldFont);
        g.drawFittedText (title.bold, titleArea, juce::Justification::centredLeft, 1, minimumTitleScale);
        return;
    }

    const auto regularFont = regularTitleFont();
    const auto [shown, squeeze] = fitTitle (title, boldFont, regularFont, layout.titleWidth);

    juce::AttributedString text;
    text.setJustification (juce::Justification::centredLeft);
    text.setWordWrap (juce::AttributedString::none);
    text.append (shown.bold, boldFont, colour);
    text.append (shown.regular, regularFont, colour);

    const juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (titleArea);
    g.addTransform (juce::AffineTransform::scale (squeeze, 1.0f, static_cast<float> (titleArea.getX()), 0.0f));
    text.draw (g, titleArea.toFloat().withWidth (static_cast<float> (layout.titleWidth) / squeeze));
}

void HeaderBar::resized()
{
    auto bounds = getLocalBounds().reduced (layout.edge, 0);
    meter.setBounds (bounds.removeFromRight (layout.meterWidth).reduced (0, layout.meterInset));

    auto controls = bounds.withTrimmedLeft (layout.titleWidth + layout.gap).withTrimmedRight (layout.gap);
    controls = controls.withSizeKeepingCentre (controls.getWidth(), layout.controlsHeight);
    menuButton.setBounds (controls.removeFromRight (28));
    controls.removeFromRight (4);
    presetBar.setBounds (controls);
}

void HeaderBar::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    presetActions.addMenuItems (menu);
    menu.addSeparator();
    menu.addItem ("Manual", showManual);
    menu.addItem ("About", [this] { prompt.showMessage ("About", aboutText); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton));
}
