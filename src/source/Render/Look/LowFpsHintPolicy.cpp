#include "Render/Look/LowFpsHintPolicy.h"

namespace Render::Look::LowFpsHint
{

std::optional<GraphicsPreset> LowerStep(GraphicsPreset preset)
{
    switch (preset)
    {
    case GraphicsPreset::Ultra:
        return GraphicsPreset::High;
    case GraphicsPreset::High:
        return GraphicsPreset::Medium;
    case GraphicsPreset::Medium:
        return GraphicsPreset::Low;
    case GraphicsPreset::Low:
    case GraphicsPreset::Auto:
    case GraphicsPreset::Custom:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<GraphicsPreset> Target(const PolicyState& state)
{
    if (!state.enabled || state.stoppedForSession)
        return std::nullopt;
    if (state.kept.has_value() && *state.kept == state.current)
        return std::nullopt;
    return LowerStep(state.current);
}

bool TargetIsClassicLook(GraphicsPreset target)
{
    return target == GraphicsPreset::Low;
}

bool StepHelped(float fpsBefore, float fpsAfter)
{
    if (fpsBefore <= 0.0f)
        return true;
    return fpsAfter >= fpsBefore * (1.0f + MinimumGainAfterStep);
}

} // namespace Render::Look::LowFpsHint
