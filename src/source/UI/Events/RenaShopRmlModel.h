#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the Rena shop window's own text: what it says and the size the native renderer would have shrunk it to
// for its box. Where it sits and its colour are the theme's.
struct RenaShopLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const RenaShopLine&) const = default;
};

// A tab of the window's pane: its label and whether it is the one shown.
struct RenaShopTab
{
    int tab = 0;
    RenaShopLine label;
    bool selected = false;

    bool operator==(const RenaShopTab&) const = default;
};

// A line of the shown tab. Its kind picks the look: 0 plain, 1 a section title, 2 good (green), 3 bad (red),
// 4 a highlighted name (bold gold), 5 a line that continues the one above (indented), 6 out of reach (grey).
// A shop line carries its index in the shop's list (-1 for the others) and whether it is the picked one.
struct RenaShopRow
{
    RenaShopLine text;
    int kind = 0;
    int item = -1;
    bool selected = false;

    bool operator==(const RenaShopRow&) const = default;
};

struct RenaShopRmlModel
{
    RenaShopLine title;
    // The Rena balance and the Rena in the bag.
    RenaShopLine balance;
    RenaShopLine bag;
    std::vector<RenaShopTab> tabs;
    std::vector<RenaShopRow> rows;
    // The last answer or why the buttons are off, on as many lines as it needs (3 at most): 0 info, 1 good, 2 bad.
    std::vector<RenaShopLine> hint;
    int hintKind = 0;
    // The hand-in button of the header.
    bool canGive = false;
    RenaShopLine giveLabel;
    // The two buttons of the Shop (Buy) and Golden Arrow (Spin) tabs: one, and as many as the second says.
    bool showButtons = false;
    bool canFirst = false;
    bool canSecond = false;
    int secondCount = 0;
    RenaShopLine firstLabel;
    RenaShopLine secondLabel;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
