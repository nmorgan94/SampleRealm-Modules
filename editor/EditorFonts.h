#pragma once

//==============================================================================
/**
 * A plugin's LookAndFeel implements this, for the text these components paint themselves and the prompt's text field.
 * Labels and buttons use the LookAndFeel's own fonts.
 */
class EditorFonts
{
public:
    virtual ~EditorFonts() = default;

    virtual juce::Font getEditorFont (float height, bool bold) = 0;

    /** From the component's LookAndFeel, or JUCE's default font when it has none to give. */
    static juce::Font of (const juce::Component& component, float height, bool bold)
    {
        if (auto* fonts = dynamic_cast<EditorFonts*> (&component.getLookAndFeel()))
            return fonts->getEditorFont (height, bold);

        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
};
