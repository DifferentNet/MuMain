#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the daily rewards window's own text: what it says and the size the native renderer would have shrunk
// it to for its box. Where it sits and its colour are the theme's.
struct DailyRewardsLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const DailyRewardsLine&) const = default;
};

// A day of the reward cycle: its label and one line per item of its reward.
struct DailyRewardsDay
{
    DailyRewardsLine label;
    std::vector<DailyRewardsLine> items;
    // The day already counted in this cycle, or is the one of today (counted or not); the theme colours them.
    bool done = false;
    bool today = false;

    bool operator==(const DailyRewardsDay&) const = default;
};

struct DailyRewardsRmlModel
{
    DailyRewardsLine title;
    // Today's progress, or while there is no answer yet the reason.
    DailyRewardsLine status;
    // What is left today, or how long until the next day.
    DailyRewardsLine detail;
    std::vector<DailyRewardsDay> days;
    // Under the days: how many rewards wait for room, or a hint.
    DailyRewardsLine footer;
    // The button that hands out the waiting rewards, shown while some wait.
    bool canTake = false;
    DailyRewardsLine takeLabel;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
