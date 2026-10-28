#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the auction window's own text: what it says and the size the native renderer would have drawn (or shrunk)
// it at for its box. Where it sits and its colour are the theme's.
struct AuctionLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const AuctionLine&) const = default;
};

// A button: its id (the click hands it back), its label, and whether it is the active tab / switched on, or off.
struct AuctionButton
{
    int id = 0;
    AuctionLine label;
    bool on = false;
    bool off = false;

    bool operator==(const AuctionButton&) const = default;
};

// A row of a list: an item's name and a second line (none for the bag's items); a click picks it, and while it is
// hovered the item's tooltip shows.
struct AuctionRow
{
    int index = 0;
    AuctionLine title;
    AuctionLine detail;
    bool selected = false;
    // The item's name in the excellent or the ancient colour.
    bool excellent = false;
    bool ancient = false;
    // A lot of the player's own, or one that waits to be taken.
    bool dim = false;
    bool live = false;

    bool operator==(const AuctionRow&) const = default;
};

// A line of the price a lot is put on sale for: [<] the jewel [>] and the count typed beside it (the model's
// count0_text ... count2_text). A line without a jewel is unused.
struct AuctionPriceStep
{
    AuctionLine kind;
    bool used = false;

    bool operator==(const AuctionPriceStep&) const = default;
};

struct AuctionRmlModel
{
    AuctionLine title;
    // 0 search, 1 my lots, 2 sell.
    int tab = 0;
    std::vector<AuctionButton> tabs;
    // The font size of the text fields.
    float fieldPx = 0.f;

    // Search: the category and my class; excellent, ancient and the order; the name typed and Find.
    std::vector<AuctionButton> filters;
    std::vector<AuctionButton> toggles;
    Rml::String searchText;
    AuctionButton find;

    // Every tab: how many, the list, the picked one's item and two lines beside its icon, two lines under it (not
    // on the Sell tab), and what to do with it.
    AuctionLine listInfo;
    std::vector<AuctionRow> rows;
    AuctionLine itemName;
    AuctionLine selFirst;
    AuctionLine selSecond;
    AuctionLine selThird;
    AuctionLine selFourth;
    // The fourth line warns (not enough jewels in the bank).
    bool selAlert = false;
    std::vector<AuctionButton> actions;

    // Sell: the days on sale, the price's lines (their counts typed by the player) and the fee.
    std::vector<AuctionButton> days;
    int priceLineCount = 0;
    AuctionPriceStep price0;
    AuctionPriceStep price1;
    AuctionPriceStep price2;
    Rml::String count0Text;
    Rml::String count1Text;
    Rml::String count2Text;
    AuctionLine fee;

    // The server's answer to the last request.
    AuctionLine status;
    bool statusBad = false;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
