#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include "BuildType.h"
#include "ParameterSpec.h"

//==============================================================================
/** JSON text without juce::JSON, for a plugin's own info beside uiInfoJson. */
namespace JsonText
{
    inline std::string quoted (const char* text)
    {
        std::string result = "\"";

        for (auto* c = text; *c != '\0'; ++c)
        {
            if (*c == '"' || *c == '\\')
                result += '\\';

            result += *c;
        }

        return result + "\"";
    }

    // Not printf, which follows the host's locale. Seven significant digits, as a float holds
    inline std::string number (float value)
    {
        std::string::size_type decimals = 6;
        long long scale = 1000000;

        for (auto limit = 10.0f; std::abs (value) >= limit && decimals > 0; limit *= 10.0f)
        {
            --decimals;
            scale /= 10;
        }

        const auto scaled = std::llround (static_cast<double> (value) * static_cast<double> (scale));
        const auto magnitude = std::llabs (scaled);
        auto text = std::string (scaled < 0 ? "-" : "") + std::to_string (magnitude / scale);

        if (const auto fraction = magnitude % scale; fraction != 0)
        {
            auto digits = std::to_string (fraction);
            digits.insert (0, decimals - digits.size(), '0');
            digits.erase (digits.find_last_not_of ('0') + 1);
            text += "." + digits;
        }

        return text;
    }
}

//==============================================================================
/** What the web editor needs to know about the plugin, as web/backend.ts's UiInfo. */
inline std::string uiInfoJson (std::span<const ParameterSpecs::Spec* const> specs, const char* version, const char* commit)
{
    using JsonText::number, JsonText::quoted;

    const auto kindName = [] (ParameterSpecs::Kind kind)
    {
        switch (kind)
        {
            case ParameterSpecs::Kind::integer: return "integer";
            case ParameterSpecs::Kind::choice:  return "choice";
            case ParameterSpecs::Kind::toggle:  return "toggle";
            case ParameterSpecs::Kind::real:    break;
        }

        return "real";
    };

    std::string json = "{\"version\":" + quoted (version) + ",\"commit\":" + quoted (commit) + ",\"build\":" + quoted (BuildType::name) + ",\"params\":[";

    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& spec = *specs[i];

        std::string choices;
        for (auto* choice : spec.choices)
            choices += (choices.empty() ? "" : ",") + quoted (choice);

        json += (i == 0 ? "{" : ",{");
        json += "\"id\":" + quoted (spec.id) + ",\"name\":" + quoted (spec.name);
        json += ",\"kind\":" + quoted (kindName (spec.kind));
        json += ",\"min\":" + number (spec.min) + ",\"max\":" + number (spec.max);
        json += ",\"defaultValue\":" + number (spec.defaultValue);
        json += ",\"step\":" + number (spec.step) + ",\"skew\":" + number (ParameterSpecs::skewOf (spec));
        json += ",\"unit\":" + quoted (spec.unit) + ",\"choices\":[" + choices + "]";

        // Only where true, so other plugins' params read as before
        if (spec.isPerformanceControl)
            json += ",\"isPerformanceControl\":true";

        json += "}";
    }

    return json + "]}";
}
