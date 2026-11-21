#include "stdafx.h"

#include "UI/Tooltip/LegacyTextListTooltip.h"

#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzInventory.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlTooltip.h"

#include <utility>
#include <vector>

namespace UI::Tooltip
{
namespace
{
UI::RmlBridge::Tooltip::LineColor ConvertColor(int color)
{
    using LineColor = UI::RmlBridge::Tooltip::LineColor;
    switch (color)
    {
    case TEXT_COLOR_BLUE: return LineColor::Blue;
    case TEXT_COLOR_GRAY: return LineColor::Gray;
    case TEXT_COLOR_RED: return LineColor::Red;
    case TEXT_COLOR_YELLOW: return LineColor::Yellow;
    case TEXT_COLOR_GREEN: return LineColor::Green;
    case TEXT_COLOR_PURPLE: return LineColor::Purple;
    case TEXT_COLOR_REDPURPLE: return LineColor::RedPurple;
    case TEXT_COLOR_VIOLET: return LineColor::Violet;
    case TEXT_COLOR_ORANGE: return LineColor::Orange;
    case TEXT_COLOR_DARKRED: return LineColor::DarkRedHighlight;
    case TEXT_COLOR_DARKBLUE: return LineColor::DarkBlueHighlight;
    case TEXT_COLOR_DARKYELLOW: return LineColor::DarkYellowHighlight;
    case TEXT_COLOR_GREEN_BLUE: return LineColor::GreenBlueHighlight;
    case TEXT_COLOR_WHITE: default: return LineColor::White;
    }
}

UI::RmlBridge::Tooltip::LineRole ConvertRole(int role)
{
    using LineRole = UI::RmlBridge::Tooltip::LineRole;
    switch (role)
    {
    case TR_NAME:
        return LineRole::Name;
    case TR_COMBAT_POWER:
        return LineRole::CombatPower;
    case TR_SECTION:
        return LineRole::Section;
    case TR_DETAIL:
        return LineRole::Detail;
    case TR_PIECE_OWNED:
        return LineRole::PieceOwned;
    case TR_PIECE_MISSING:
        return LineRole::PieceMissing;
    case TR_NONE:
    default:
        return LineRole::None;
    }
}

// With `withRoles`, each text line also takes its role from TextListRole (item information only).
std::vector<UI::RmlBridge::Tooltip::Line> BuildLines(int lineCount, bool withRoles)
{
    using Line = UI::RmlBridge::Tooltip::Line;
    // The rich layout adds the kind and hint lines and a spacer under the header and before each section header (an
    // ancient with its set has up to five), so arranging it never grows the vector.
    constexpr size_t RichLayoutSpareLines = 8;
    std::vector<Line> lines;
    lines.reserve(static_cast<size_t>(lineCount) + (withRoles ? RichLayoutSpareLines : 0));

    for (int i = 0; i < lineCount; ++i)
    {
        if (TextList[i][0] == L'\0')
            break;

        Line line;
        if (TextList[i][0] == L'\n')
            line.kind = Line::Kind::HalfSpacer;
        else if (TextList[i][0] == L' ' && TextList[i][1] == L'\0')
            line.kind = Line::Kind::FullSpacer;
        else
        {
            line.text = StringUtils::WideToNarrow(TextList[i]);
            line.bold = TextBold[i] != 0;
            line.color = ConvertColor(TextListColor[i]);
            if (withRoles)
                line.role = ConvertRole(TextListRole[i]);
        }
        lines.push_back(std::move(line));
    }
    return lines;
}
} // namespace

const void* ItemInfoOwner()
{
    static const char token = 0;
    return &token;
}

bool UsesRichItemLayout()
{
    return UI::RmlBridge::ThemeHasRichItemTooltip() && !UI::Mobile::IsPhoneView();
}

namespace
{
// The rich layout of an item information tooltip; the lines stay as they are when it has no name line.
void ApplyRichLayout(UI::RmlBridge::Tooltip::Config& config, const ItemPresentation& presentation)
{
    if (!ArrangeRich(config.lines, presentation.layout))
        return;

    config.rich = true;
    config.rarity = presentation.rarity;
    config.band = presentation.band;
}

UI::RmlBridge::Tooltip::Config TextListConfig(int lineCount, float anchorX, float anchorY, Placement placement,
                                              const void* owner, const ItemPresentation* presentation)
{
    const bool rich = presentation != nullptr && UsesRichItemLayout();
    UI::RmlBridge::Tooltip::Config config;
    config.lines = BuildLines(lineCount, rich);
    if (rich)
        ApplyRichLayout(config, *presentation);
    config.anchorX = anchorX;
    config.anchorY = anchorY;
    config.centerHorizontally = true;
    config.anchor = placement == Placement::Above ? UI::RmlBridge::Tooltip::AnchorPoint::AboveLeft
                                                   : UI::RmlBridge::Tooltip::AnchorPoint::BelowLeft;
    config.textAlign = UI::RmlBridge::Tooltip::Config::TextAlign::Center;
    // Every item information tooltip is drawn from its window's Render() while the item is hovered.
    config.refreshEachFrame = owner == ItemInfoOwner();
    return config;
}
} // namespace

void ShowLegacyTextList(int lineCount, float anchorX, float anchorY, Placement placement, const void* owner,
                        const ItemPresentation* presentation)
{
    UI::RmlBridge::Tooltip::Show(TextListConfig(lineCount, anchorX, anchorY, placement, owner, presentation), owner);
}

void ShowLegacyTextListAboveRow(int lineCount, const AboveRow& spot, const void* owner,
                                const ItemPresentation* presentation)
{
    UI::RmlBridge::Tooltip::Config config =
        TextListConfig(lineCount, spot.x, spot.bottomY, Placement::Above, owner, presentation);
    config.besideX = spot.besideX;
    UI::RmlBridge::Tooltip::Show(config, owner);
}

void HideLegacyTextList(const void* owner)
{
    UI::RmlBridge::Tooltip::Hide(owner);
}
} // namespace UI::Tooltip
