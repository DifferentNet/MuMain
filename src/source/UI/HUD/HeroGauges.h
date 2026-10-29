#pragma once

// The hero's gauges as the HUD shows them, for every HUD that draws them: the PC bar
// (CMainFrameWindow) and the phone's hero frame (UI::Mobile::HeroFrame).

#include <cstdint>

namespace UI::HUD::HeroGauges
{
// What the hero has now, and at most.
struct Gauge
{
    std::uint32_t current = 0;
    std::uint32_t max = 0;

    [[nodiscard]] float Fraction() const { return max > 0 ? static_cast<float>(current) / max : 0.f; }
};

// A master's own maxima once master level is reached. AG and SD count at least 1 at most.
[[nodiscard]] Gauge Life();
[[nodiscard]] Gauge Mana();
[[nodiscard]] Gauge SkillMana();
[[nodiscard]] Gauge Shield();

// The level the EXP gauge counts (the master level once it is active), the experience and the
// amount the next level needs, and how far the hero is from this level's start to the next.
struct Experience
{
    std::int64_t level = 0;
    std::int64_t current = 0;
    std::int64_t next = 0;
    double progress = 0.0;
};
[[nodiscard]] Experience ReadExperience();
} // namespace UI::HUD::HeroGauges
