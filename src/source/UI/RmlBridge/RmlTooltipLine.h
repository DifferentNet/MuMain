#pragma once

#include <string>

// The lines of the shared tooltip (UI/RmlBridge/RmlTooltip.h), standard library only so the pure layout code
// (UI/Tooltip/ItemTooltipLayout) and its tests can use them.
namespace UI::RmlBridge::Tooltip
{
// Superset of every color capability across the five prior mechanisms -- the item tooltip's
// own palette (TEXT_COLOR_*, Core/Globals/_define.h) was already the richest, so nothing loses
// capability by unifying onto it. The four *Highlight values keep their original meaning: text
// renders in the color RenderTipTextList() used for it (white, except GreenBlue's own green),
// on top of a colored highlight bar behind the line.
enum class LineColor
{
    White,
    Blue,
    Gray,
    Red,
    Yellow,
    Green,
    Purple,
    RedPurple,
    Violet,
    Orange,
    DarkRedHighlight,
    DarkBlueHighlight,
    DarkYellowHighlight,
    GreenBlueHighlight,
};

// What an item tooltip line is, for a theme that lays item tooltips out richly (RichItemTooltip=1). Every
// other tooltip leaves it None.
enum class LineRole
{
    None,
    Name,
    Kind,        // "Ancient · Armour" under the name
    CombatPower, // the item's own Combat Power
    Section,     // a block header: set options, Power of the Ancients, socket options
    Detail,      // a line the short view may hide (an inactive bonus)
    PieceOwned,  // a set piece the hero has
    PieceMissing,
    Hint, // "Hold Shift for details"
};

struct Line
{
    // A spacer line renders no text at all -- `text` is ignored for these two kinds. Replaces
    // RenderItemInfo()'s old convention of sniffing the first character of a native text
    // buffer ('\n' for a half-height spacer, a lone ' ' for a full-height one) with an
    // explicit field.
    enum class Kind
    {
        Text,
        HalfSpacer,
        FullSpacer,
    };

    std::string text;
    LineColor color = LineColor::White;
    bool bold = false;
    Kind kind = Kind::Text;
    LineRole role = LineRole::None;
    // The rich layout's header (name, kind, Combat Power), and its last line.
    bool head = false;
    bool headLast = false;
};
} // namespace UI::RmlBridge::Tooltip
