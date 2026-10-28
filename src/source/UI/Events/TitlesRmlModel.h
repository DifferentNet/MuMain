#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the titles window's own text: what it says and the size the native renderer would have shrunk it to for
// its box. Where it sits and its colour are the theme's.
struct TitlesLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const TitlesLine&) const = default;
};

// A tab over the list: "Mine" or "All".
struct TitlesTab
{
    TitlesLine label;
    bool selected = false;

    bool operator==(const TitlesTab&) const = default;
};

// A row of the list: a title (or "No title"), how long it lasts or lasts left, the bonus of its rarity, how to get it
// while it is not owned.
struct TitlesRow
{
    // The row's place in the list, for a click (titles_wear).
    int index = 0;
    TitlesLine name;
    TitlesLine detail;
    TitlesLine bonus;
    TitlesLine hint;
    TitlesLine mark;
    // The plate's colour (GameLogic::Titles::Rarity); -1 for the "No title" row.
    int rarity = 0;
    bool owned = false;
    bool worn = false;

    bool operator==(const TitlesRow&) const = default;
};

struct TitlesRmlModel
{
    TitlesLine title;
    std::vector<TitlesTab> tabs;
    // The worn title, and whether a new one goes on by itself; while there is no answer yet the reason.
    TitlesLine status;
    TitlesLine detail;
    std::vector<TitlesRow> rows;
    // Under the list: an empty "Mine" tab's note, or how many titles are owned.
    TitlesLine footer;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
