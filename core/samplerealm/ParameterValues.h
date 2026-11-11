#pragma once

#include <cmath>
#include "ParameterSpec.h"

//==============================================================================
/**
 * A value for each spec, starting at its default, for where there's no APVTS: the wasm engine.
 * Call it with a spec for that spec's value, as a plugin's settings function expects.
 */
template <size_t Count>
class ParameterValues
{
public:
    explicit ParameterValues (const std::array<const ParameterSpecs::Spec*, Count>& specsInHostOrder)
        : specs (specsInHostOrder)
    {
        std::ranges::transform (specs, values.begin(), [] (const ParameterSpecs::Spec* spec) { return spec->defaultValue; });
    }

    /**
     * By position in the host's order. A position that isn't there, or a NaN, is ignored. As the
     * APVTS, the value is kept inside the spec's range, and whole for an integer or a choice.
     */
    void set (int index, float value)
    {
        if (index < 0 || index >= static_cast<int> (Count) || std::isnan (value))
            return;

        const auto position = static_cast<size_t> (index);
        const auto& spec = *specs[position];
        const auto inRange = std::clamp (value, spec.min, spec.max);
        values[position] = spec.kind == ParameterSpecs::Kind::real ? inRange : std::round (inRange);
    }

    float operator() (const ParameterSpecs::Spec& spec) const
    {
        const auto found = std::ranges::find (specs, &spec);
        return found != specs.end() ? values[static_cast<size_t> (found - specs.begin())] : spec.defaultValue;
    }

private:
    std::array<const ParameterSpecs::Spec*, Count> specs;
    std::array<float, Count> values;
};
