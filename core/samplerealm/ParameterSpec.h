#pragma once

#include <algorithm>
#include <array>
#include <span>

//==============================================================================
/** What a parameter is. A plugin's ParameterSpecs.h adds its own specs and `all`, the host's order. */
namespace ParameterSpecs
{
    enum class Kind { real, integer, choice, toggle };

    struct Spec
    {
        const char* id;
        const char* name;
        Kind kind;
        float min, max, defaultValue;
        float step = 0.0f;
        float skew = 1.0f;
        const char* unit = "";
        std::span<const char* const> choices = {};
        bool automatable = true;
        // Played live: presets don't save it, and moving it isn't an edit
        bool isPerformanceControl = false;
    };

    constexpr Spec integer (const char* id, const char* name, int min, int max, int defaultValue, const char* unit = "")
    {
        return { id, name, Kind::integer, static_cast<float> (min), static_cast<float> (max),
                 static_cast<float> (defaultValue), 1.0f, 1.0f, unit };
    }

    constexpr Spec choice (const char* id, const char* name, std::span<const char* const> choices, int defaultValue)
    {
        return { id, name, Kind::choice, 0.0f, static_cast<float> (choices.size() - 1),
                 static_cast<float> (defaultValue), 1.0f, 1.0f, "", choices };
    }

    /** An on/off parameter: 0 or 1. */
    constexpr Spec toggle (const char* id, const char* name, bool defaultValue)
    {
        return { id, name, Kind::toggle, 0.0f, 1.0f, defaultValue ? 1.0f : 0.0f, 1.0f, 1.0f };
    }

    /** For a display switch, say, which hosts shouldn't automate: notAutomatable (toggle (...)) */
    constexpr Spec notAutomatable (Spec spec)
    {
        spec.automatable = false;
        return spec;
    }

    /** For a control played live, like pitch bend: performanceControl ({ ... }). PresetManager::Config::ignoredParameterIds takes its id. */
    constexpr Spec performanceControl (Spec spec)
    {
        spec.isPerformanceControl = true;
        return spec;
    }

    /** Integer, choice and toggle specs need step 1, a choice spec's range is 0 to its last choice, and a toggle is 0 or 1 with no choices. */
    constexpr bool areValid (std::span<const Spec* const> specs)
    {
        return std::ranges::all_of (specs, [] (const Spec* spec)
        {
            switch (spec->kind)
            {
                case Kind::real:    return true;
                case Kind::integer: return spec->step == 1.0f;
                case Kind::toggle:  return spec->step == 1.0f && spec->min == 0.0f && spec->max == 1.0f
                                        && (spec->defaultValue == 0.0f || spec->defaultValue == 1.0f) && spec->choices.empty();
                case Kind::choice:  break;
            }

            return spec->step == 1.0f && spec->min == 0.0f && static_cast<size_t> (spec->max) == spec->choices.size() - 1;
        });
    }
}
