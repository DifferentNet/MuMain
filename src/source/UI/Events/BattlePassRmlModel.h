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

// A chest of the daily quests: the day's under its mark on the day's bar, the week's on the week's line. Its hint
// says what it holds.
struct BattlePassChestBox
{
    // "I", "II", "III"; the week's: a word.
    Rml::String label;
    Rml::String hint;
    // Its id ("bp_chest_1"-"bp_chest_3", the week's "bp_chest_w"), and where its mark is on the day's bar (0-100).
    Rml::String id;
    float at = 0.f;
    // Given; enough quests done for it (the server gives it then, so this is mostly the same).
    bool got = false;
    bool reached = false;

    bool operator==(const BattlePassChestBox&) const = default;
};

// A line of the quests tab: a section's heading, a quest, a line of the season's result, or one of the daily quests'
// lines (the day's bar with its chests, the note on points, the week's line with its reward).
struct BattlePassQuestRow
{
    // 0 a quest, 1 a section's heading, 2 a week's heading inside the weekly ones, 3 a result line, 4 the day's bar,
    // 5 a note, 6 the week's line of the daily quests.
    int type = 0;
    Rml::String text;
    // A quest's progress ("37 / 2500"); a result line's value; the day's bar: "4 / 10".
    Rml::String detail;
    // A heading's time left, from the model's own timers (they change every second, the rows don't): 0 none, 1 the
    // day's, 2 the week's, 3 the season's.
    int timer = 0;
    // "+200"; empty for a quest that gives no points.
    Rml::String points;
    // The progress bar's fill, 0-100 (the day's bar: quests done of the day's).
    float fill = 0.f;
    bool done = false;
    // A daily quest's difficulty (1 easy, 2 medium, 3 hard; 0 none) and its word.
    int difficulty = 0;
    Rml::String difficultyLabel;
    // A daily quest that can be replaced today, and its slot for /bpreroll.
    bool canReroll = false;
    int slot = 0;
    // The day's bar: what the day's quests give in turn.
    Rml::String hint;
    // The day's bar: its chests (a mark and a box each); the week's line: the week's chest. A row's own, so the
    // other rows hold none.
    std::vector<BattlePassChestBox> chests;

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
    // The headings' times left ("new in 5:12:30"), kept apart from the rows so a second's tick touches only them.
    Rml::String dayTimer;
    Rml::String weekTimer;
    Rml::String seasonTimer;

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
