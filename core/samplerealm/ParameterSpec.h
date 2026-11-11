#pragma once

#include <algorithm>
#include <array>
#include <span>

//==============================================================================
/** What a parameter is. A plugin's ParameterSpecs.h adds its own specs and `all`, the host's order. */
namespace ParameterSpecs
{
    enum class Kind { real, integer, choice };

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

    /** Integer and choice specs need step 1, and a choice spec's range is 0 to its last choice. */
    constexpr bool areValid (std::span<const Spec* const> specs)
    {
        return std::ranges::all_of (specs, [] (const Spec* spec)
        {
            if (spec->kind == Kind::real)
                return true;

            return spec->step == 1.0f
                && (spec->kind == Kind::integer
                    || (spec->min == 0.0f && static_cast<size_t> (spec->max) == spec->choices.size() - 1));
        });
    }
}
