#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include "BuildType.h"
#include "ParameterSpec.h"

//==============================================================================
/** What the web editor needs to know about the plugin, as web/backend.ts's UiInfo. */
inline std::string uiInfoJson (std::span<const ParameterSpecs::Spec* const> specs, const char* version, const char* commit)
{
    const auto quoted = [] (const char* text)
    {
        std::string result = "\"";

        for (auto* c = text; *c != '\0'; ++c)
        {
            if (*c == '"' || *c == '\\')
                result += '\\';

            result += *c;
        }

        return result + "\"";
    };

    // Not printf, which follows the host's locale
    const auto number = [] (float value)
    {
        const auto scaled = std::llround (static_cast<double> (value) * 10000.0);
        const auto magnitude = std::llabs (scaled);
        auto text = std::string (scaled < 0 ? "-" : "") + std::to_string (magnitude / 10000);

        if (const auto fraction = magnitude % 10000; fraction != 0)
        {
            auto digits = std::to_string (fraction);
            digits.insert (0, 4 - digits.size(), '0');
            digits.erase (digits.find_last_not_of ('0') + 1);
            text += "." + digits;
        }

        return text;
    };

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
        json += ",\"step\":" + number (spec.step) + ",\"skew\":" + number (spec.skew);
        json += ",\"unit\":" + quoted (spec.unit) + ",\"choices\":[" + choices + "]}";
    }

    return json + "]}";
}
