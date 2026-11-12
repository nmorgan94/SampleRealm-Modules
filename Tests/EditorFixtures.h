#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace EditorFixtures
{
    template <typename T>
    std::vector<T*> descendantsOf (juce::Component& parent)
    {
        std::vector<T*> found;

        for (auto* child : parent.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.push_back (match);

            const auto below = descendantsOf<T> (*child);
            found.insert (found.end(), below.begin(), below.end());
        }

        return found;
    }

    // A parent the size of a small editor, for overlays
    struct OverlayFixture
    {
        OverlayFixture()   { parent.setSize (440, 312); }

        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        juce::Component parent;
    };
}
