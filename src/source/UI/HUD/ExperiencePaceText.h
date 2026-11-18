#pragma once

#include <cstdint>
#include <optional>
#include <string>

// The time to the next level as the player reads it (UI/HUD/ExperiencePace.h gives the pace): the EXP bar's
// tooltip on a PC and the phone's EXP line.
namespace UI::HUD::ExperiencePace
{
// "1h 20m" or "20 min" in the player's language for `remaining` experience at `perHour`; nothing when there is no
// time to show (no pace, nothing left, more than MaxShownMinutes).
[[nodiscard]] std::optional<std::wstring> TimeToGainText(std::int64_t remaining, double perHour);
} // namespace UI::HUD::ExperiencePace
