#pragma once

#include "Data/GameConfig/GameConfigValidation.h"

#include <optional>

// Which graphics preset the low frame rate hint offers, and when it offers none. Pure: the caller passes the
// player's preset and choices. The hint only ever lowers the preset one step (Ultra, High, Medium, Low); Low, Auto
// and Custom (the player's own mix) get none. See docs/enhanced-look.md, "Low frame rate hint".
namespace Render::Look::LowFpsHint
{

// What decides whether a hint is offered.
struct PolicyState
{
    GraphicsPreset current = GraphicsPreset::Auto;
    // [Render] LowFpsHint: off after "Don't ask again", on again from Options.
    bool enabled = true;
    // [Render] LowFpsHintKept: the preset the player chose to keep; no hint while it is the current one.
    std::optional<GraphicsPreset> kept;
    // A lower step didn't help this session (the game is held back by something else): no more hints until a restart.
    bool stoppedForSession = false;
};

// The preset one step below `preset`, or none for Low, Auto and Custom. The one place the steps live.
[[nodiscard]] std::optional<GraphicsPreset> LowerStep(GraphicsPreset preset);

// The preset the hint offers now, or none.
[[nodiscard]] std::optional<GraphicsPreset> Target(const PolicyState& state);

// Low is the classic look: the hint says so when it offers Low.
[[nodiscard]] bool TargetIsClassicLook(GraphicsPreset target);

// A lower step must raise the median frame rate by this share, or the hints stop for the session.
inline constexpr float MinimumGainAfterStep = 0.15f;

// Whether a step that took the median from `fpsBefore` to `fpsAfter` helped enough to offer another one later.
[[nodiscard]] bool StepHelped(float fpsBefore, float fpsAfter);

} // namespace Render::Look::LowFpsHint
