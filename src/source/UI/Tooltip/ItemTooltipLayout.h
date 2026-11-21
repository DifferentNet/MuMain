#pragma once

#include "UI/RmlBridge/RmlTooltipLine.h"

#include <string_view>
#include <vector>

// The rich item tooltip layout (theme capability RichItemTooltip=1, the Skynet theme's step C): the same lines the
// item tooltip always had, put in a header and blocks for the theme to style. Pure, so it is tested on its own.
namespace UI::Tooltip
{
// The texts are views (ItemTooltipTexts.h keeps them per language): they must outlive the ArrangeRich call only,
// which copies what it shows into the lines.
struct RichInput
{
    // The line under the name ("Ancient · Armour"); none when empty.
    std::string_view kind;
    // Shift held, or touch or a gamepad in use: nothing is hidden.
    bool detailed = false;
    // The short view's last line ("Hold Shift for details").
    std::string_view hint;
};

// Arranges an item tooltip's lines in place:
// - the header: the name, the kind line and the item's own Combat Power, flagged `head` (the last also `headLast`);
// - then the rest in its order (an NPC shop's price lines first), each run of blank lines one half spacer (the
//   theme draws it as a divider), none at either end, exactly one before each section header and none after it;
// - unless `detailed`, the inactive bonus lines (LineRole::Detail) are left out when there are 3 or more, and the
//   hint closes the tooltip; a section header left with nothing under it goes too;
// - set pieces start with a tick or a dash.
// Arranging an arranged list again gives the same list. Returns false, the lines untouched, without a name line.
bool ArrangeRich(std::vector<RmlBridge::Tooltip::Line>& lines, const RichInput& input);

// The scale that fits a rich tooltip into `availablePx` of height: `scaledPx` is the part that shrinks with the text
// (its lines and padding), `fixedPx` the part that does not (the frame, the shadow and the glow past it). 1 when it
// fits or nothing is measured; never below `minScale`, the tooltip then stays taller than `availablePx`.
[[nodiscard]] float FitScale(float scaledPx, float fixedPx, float availablePx, float minScale);
} // namespace UI::Tooltip
