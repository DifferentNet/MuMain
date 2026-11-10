#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the Grand Reset shop window's own text: what it says and the size the native renderer would have shrunk it
// to for its box. Where it sits and its colour are the theme's.
struct GrandShopLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const GrandShopLine&) const = default;
};

// A row of the set list: a set's name (or the Jewel of Ancient), whether it is a high-tier set (the "top" mark) and
// whether it is the shown one. `index` is its place in the list; the click carries it back.
struct GrandShopSet
{
    GrandShopLine name;
    bool top = false;
    int index = 0;
    bool selected = false;

    bool operator==(const GrandShopSet&) const = default;
};

// A piece of the shown set: the id of the box its 3D icon is drawn into, its name and price, the Buy button's state
// and, while the button is off, why. `index` is its place in the shown set; the click carries it back.
struct GrandShopItem
{
    Rml::String id;
    GrandShopLine name;
    GrandShopLine price;
    int index = 0;
    bool canBuy = false;
    Rml::String hint;

    bool operator==(const GrandShopItem&) const = default;
};

struct GrandShopRmlModel
{
    GrandShopLine title;
    // The Grand Marks balance, or the "asking" / "no answer" text while there is no data.
    GrandShopLine marks;
    bool hasInfo = false;
    std::vector<GrandShopSet> sets;
    std::vector<GrandShopItem> items;
    GrandShopLine topLabel;
    GrandShopLine buyLabel;
    // The last answer or why a button is off, on as many lines as it needs (3 at most): 0 info, 1 good, 2 bad.
    std::vector<GrandShopLine> hint;
    int hintKind = 0;
    // The standing note about the account binding.
    std::vector<GrandShopLine> note;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
