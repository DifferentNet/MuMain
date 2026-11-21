#include "UI/Tooltip/ItemTooltipLayout.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace UI::Tooltip
{
namespace
{
using RmlBridge::Tooltip::Line;
using RmlBridge::Tooltip::LineColor;
using RmlBridge::Tooltip::LineRole;

// One or two inactive lines are shown all the same: hiding them saves nothing.
constexpr std::size_t MinHiddenDetails = 3;
constexpr std::string_view OwnedMark = "\xE2\x9C\x93 ";   // U+2713 check mark (DejaVu Sans has it)
constexpr std::string_view MissingMark = "\xE2\x80\x93 "; // U+2013 en dash

bool IsSpacer(const Line& line)
{
    return line.kind != Line::Kind::Text;
}

bool HasRole(const Line& line, LineRole role)
{
    return line.kind == Line::Kind::Text && line.role == role;
}

Line HalfSpacer()
{
    Line line;
    line.kind = Line::Kind::HalfSpacer;
    return line;
}

Line TextLine(std::string_view text, LineColor color, LineRole role)
{
    Line line;
    line.text.assign(text);
    line.color = color;
    line.role = role;
    return line;
}

// Takes out what an earlier arrange added (the kind and hint lines, the header flags); true when it had the hint.
bool StripArranged(std::vector<Line>& lines)
{
    const bool hadHint =
        std::any_of(lines.begin(), lines.end(), [](const Line& line) { return HasRole(line, LineRole::Hint); });
    std::erase_if(lines,
                  [](const Line& line) { return HasRole(line, LineRole::Kind) || HasRole(line, LineRole::Hint); });
    for (Line& line : lines)
    {
        line.head = false;
        line.headLast = false;
    }
    return hadHint;
}

// True when it left the inactive lines out.
bool HideDetails(std::vector<Line>& lines, bool detailed)
{
    if (detailed)
        return false;

    const auto details =
        std::count_if(lines.begin(), lines.end(), [](const Line& line) { return HasRole(line, LineRole::Detail); });
    if (static_cast<std::size_t>(details) < MinHiddenDetails)
        return false;

    std::erase_if(lines, [](const Line& line) { return HasRole(line, LineRole::Detail); });
    return true;
}

// Name, kind, Combat Power at the top; returns how many lines the header has.
std::size_t MoveHeader(std::vector<Line>& lines, std::string_view kind)
{
    const auto name =
        std::find_if(lines.begin(), lines.end(), [](const Line& line) { return HasRole(line, LineRole::Name); });
    std::rotate(lines.begin(), name, name + 1);
    std::size_t size = 1;

    const auto combatPower = std::find_if(lines.begin() + 1, lines.end(),
                                          [](const Line& line) { return HasRole(line, LineRole::CombatPower); });
    if (combatPower != lines.end())
    {
        std::rotate(lines.begin() + 1, combatPower, combatPower + 1);
        ++size;
    }

    if (!kind.empty())
    {
        lines.insert(lines.begin() + 1, TextLine(kind, LineColor::White, LineRole::Kind));
        ++size;
    }

    for (std::size_t i = 0; i < size; ++i)
    {
        lines[i].head = true;
        lines[i].headLast = i + 1 == size;
    }
    return size;
}

// True when only blank lines follow the section header at `section` up to the next header, the hint or the end.
bool IsEmptySection(const std::vector<Line>& lines, std::size_t section)
{
    for (std::size_t i = section + 1; i < lines.size(); ++i)
    {
        if (!IsSpacer(lines[i]))
            return HasRole(lines[i], LineRole::Section) || HasRole(lines[i], LineRole::Hint);
    }
    return true;
}

// The section headers left with nothing under them once the inactive lines are out (an ancient's "Power of the
// Ancients" and "Full set bonus" while nothing of either is on); ArrangeBody then joins the blank lines around them.
void DropEmptySections(std::vector<Line>& lines, std::size_t bodyStart)
{
    for (std::size_t i = lines.size(); i-- > bodyStart;)
    {
        if (HasRole(lines[i], LineRole::Section) && IsEmptySection(lines, i))
            lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

// One half spacer under the header and before each section, none after a section or at the end; the other blank
// lines between two text lines become one half spacer.
void ArrangeBody(std::vector<Line>& lines, std::size_t bodyStart)
{
    std::size_t out = bodyStart;
    bool spacerPending = true;
    bool afterSection = false;
    for (std::size_t i = bodyStart; i < lines.size(); ++i)
    {
        if (IsSpacer(lines[i]))
        {
            spacerPending = spacerPending || !afterSection;
            continue;
        }

        const bool section = HasRole(lines[i], LineRole::Section);
        if (spacerPending || section)
        {
            // Every slot below i is free; at i itself the spacer has to be put in.
            if (out == i)
            {
                lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(out), HalfSpacer());
                ++i;
            }
            else
            {
                lines[out] = HalfSpacer();
            }
            ++out;
            spacerPending = false;
        }

        if (out != i)
            lines[out] = std::move(lines[i]);
        ++out;
        afterSection = section;
    }
    lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(out), lines.end());
}

void Prefix(std::string& text, std::string_view mark)
{
    if (!std::string_view(text).starts_with(mark))
        text.insert(0, mark);
}

void MarkPieces(std::vector<Line>& lines)
{
    for (Line& line : lines)
    {
        if (HasRole(line, LineRole::PieceOwned))
            Prefix(line.text, OwnedMark);
        else if (HasRole(line, LineRole::PieceMissing))
            Prefix(line.text, MissingMark);
    }
}
} // namespace

bool ArrangeRich(std::vector<Line>& lines, const RichInput& input)
{
    const bool hasName =
        std::any_of(lines.begin(), lines.end(), [](const Line& line) { return HasRole(line, LineRole::Name); });
    if (!hasName)
        return false;

    const bool hadHint = StripArranged(lines);
    const bool hidDetails = HideDetails(lines, input.detailed);
    const std::size_t headerSize = MoveHeader(lines, input.kind);
    if (hidDetails)
        DropEmptySections(lines, headerSize);
    ArrangeBody(lines, headerSize);
    MarkPieces(lines);

    if ((hidDetails || hadHint) && !input.detailed && !input.hint.empty())
        lines.push_back(TextLine(input.hint, LineColor::Gray, LineRole::Hint));
    return true;
}

float FitScale(float scaledPx, float fixedPx, float availablePx, float minScale)
{
    if (scaledPx <= 0.0f || availablePx <= 0.0f || scaledPx + fixedPx <= availablePx)
        return 1.0f;
    const float fit = (availablePx - fixedPx) / scaledPx;
    return std::max(minScale, std::min(fit, 1.0f));
}
} // namespace UI::Tooltip
