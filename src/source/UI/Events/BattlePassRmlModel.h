#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the battle pass window's own text: what it says and the size the native renderer would have shrunk it
// to for its box. Where it sits and its colour are the theme's.
struct BattlePassLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const BattlePassLine&) const = default;
};

// An item of a level's reward. A game item is drawn live in 3D by C++ over its box, with the game's item tooltip;
// the others (a random excellent item, a sign of the season, an item not in the game yet) show a sign with a hint.
struct BattlePassIcon
{
    // The sign's text; empty for a game item.
    Rml::String sign;
    // "x5" over the box's corner; empty for one.
    Rml::String count;
    Rml::String hint;
    // The element's id, which C++ finds it by to draw the item and place its tooltip, and its index among all
    // the boxes, which the pointer's hover reports.
    Rml::String id;
    int index = 0;

    bool operator==(const BattlePassIcon&) const = default;
};

// A level of the track: its number and its reward's items. A reached level not claimed yet is claimed with a click.
struct BattlePassLevelRow
{
    int level = 0;
    Rml::String label;
    Rml::String id;
    std::vector<BattlePassIcon> icons;
    // The reward in words where it has no items (a sign of a later season).
    Rml::String note;
    Rml::String hint;
    bool claimed = false;
    bool claimable = false;
    // The next level to reach.
    bool next = false;

    bool operator==(const BattlePassLevelRow&) const = default;
};

// A line of the quests tab: a section's heading, a quest, or a line of the season's result.
struct BattlePassQuestRow
{
    // 0 a quest, 1 a section's heading, 2 a week's heading inside the weekly ones, 3 a result line.
    int type = 0;
    Rml::String text;
    // A heading's time left; a quest's progress ("37 / 2500"); a result line's value.
    Rml::String detail;
    // "+200".
    Rml::String points;
    // The progress bar's fill, 0-100.
    float fill = 0.f;
    bool done = false;
    // A daily quest that can be replaced today, and its slot for /bpreroll.
    bool canReroll = false;
    int slot = 0;

    bool operator==(const BattlePassQuestRow&) const = default;
};

struct BattlePassRmlModel
{
    float textPx = 0.f;
    float boldPx = 0.f;

    BattlePassLine title;
    // The season's name and its time left, side by side.
    BattlePassLine season;
    BattlePassLine time;
    // The level reached and the season's points.
    BattlePassLine level;
    BattlePassLine points;
    // The gauge of the current level: its fill (0-100) and "430 / 1000".
    float gaugeFill = 0.f;
    BattlePassLine gauge;

    // 0 the rewards, 1 the quests.
    int tab = 0;
    BattlePassLine tabRewards;
    BattlePassLine tabQuests;

    // While there is nothing to show: asking, no answer, the pass is off.
    BattlePassLine status;
    bool hasInfo = false;

    std::vector<BattlePassLevelRow> levels;
    // Under the track: the bonus levels.
    Rml::String bonus;
    std::vector<BattlePassQuestRow> quests;

    // The quests tab's button that hands out the rewards that wait for room, shown while some wait.
    bool canTake = false;
    Rml::String takeLabel;

    // The button at the bottom: claims all reached levels, or else hands out the rewards that wait.
    bool canClaim = false;
    BattlePassLine claimLabel;

    Rml::String rerollLabel;
    Rml::String rerollTooltip;
    Rml::String exitTooltip;
};
} // namespace mu::ui::window
