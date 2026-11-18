#include "GameLogic/Items/JunkMarkSet.h"

#include <algorithm>
#include <charconv>

namespace GameLogic::Items::JunkMarks
{
namespace
{
constexpr char PairSeparator = ';';
constexpr char FieldSeparator = ':';
constexpr int HexBase = 16;

bool ParseMark(std::string_view entry, JunkMarks::Mark& mark)
{
    const std::size_t colon = entry.find(FieldSeparator);
    if (colon == std::string_view::npos)
        return false;
    const std::string_view slotText = entry.substr(0, colon);
    const std::string_view printText = entry.substr(colon + 1);
    int slot = -1;
    Fingerprint print = 0;
    const auto slotEnd = slotText.data() + slotText.size();
    const auto printEnd = printText.data() + printText.size();
    const auto slotResult = std::from_chars(slotText.data(), slotEnd, slot);
    const auto printResult = std::from_chars(printText.data(), printEnd, print, HexBase);
    if (slotResult.ec != std::errc{} || slotResult.ptr != slotEnd || printResult.ec != std::errc{} ||
        printResult.ptr != printEnd)
        return false;
    if (slot < 0 || print == 0)
        return false;
    mark = {slot, print};
    return true;
}
} // namespace

bool JunkMarkSet::IsMarked(int slot, Fingerprint print) const
{
    return std::any_of(m_marks.begin(), m_marks.end(),
                       [slot, print](const JunkMarks::Mark& mark) { return mark.slot == slot && mark.print == print; });
}

bool JunkMarkSet::Mark(int slot, Fingerprint print)
{
    if (slot < 0 || print == 0)
        return false;
    Unmark(slot);
    if (m_marks.size() >= MaxMarks)
        return false;
    m_marks.push_back({slot, print});
    return true;
}

void JunkMarkSet::Unmark(int slot)
{
    std::erase_if(m_marks, [slot](const JunkMarks::Mark& mark) { return mark.slot == slot; });
}

void JunkMarkSet::Move(int from, int to)
{
    if (from == to)
        return;
    Unmark(to);
    for (JunkMarks::Mark& mark : m_marks)
    {
        if (mark.slot == from)
            mark.slot = to;
    }
}

void JunkMarkSet::Settle(const std::vector<BagItem>& bag, bool followSort)
{
    const auto holds = [&bag](int slot, Fingerprint print)
    {
        return std::any_of(bag.begin(), bag.end(),
                           [slot, print](const BagItem& item) { return item.slot == slot && item.print == print; });
    };
    std::vector<JunkMarks::Mark> kept;
    std::vector<JunkMarks::Mark> lost;
    for (const JunkMarks::Mark& mark : m_marks)
        (holds(mark.slot, mark.print) ? kept : lost).push_back(mark);
    if (followSort)
    {
        const auto isTaken = [&kept](int slot)
        { return std::any_of(kept.begin(), kept.end(), [slot](const JunkMarks::Mark& m) { return m.slot == slot; }); };
        for (const JunkMarks::Mark& mark : lost)
        {
            const auto found = std::find_if(bag.begin(), bag.end(), [&](const BagItem& item)
                                            { return item.print == mark.print && !isTaken(item.slot); });
            if (found != bag.end())
                kept.push_back({found->slot, mark.print});
        }
    }
    m_marks = std::move(kept);
}

void JunkMarkSet::Clear()
{
    m_marks.clear();
}

std::string JunkMarkSet::Serialize() const
{
    std::string text;
    char buffer[32];
    for (const JunkMarks::Mark& mark : m_marks)
    {
        auto end = std::to_chars(buffer, buffer + sizeof(buffer), mark.slot).ptr;
        *end++ = FieldSeparator;
        end = std::to_chars(end, buffer + sizeof(buffer), mark.print, HexBase).ptr;
        text.append(buffer, end);
        text.push_back(PairSeparator);
    }
    return text;
}

JunkMarkSet JunkMarkSet::Parse(std::string_view text)
{
    JunkMarkSet set;
    while (!text.empty())
    {
        const std::size_t end = text.find(PairSeparator);
        const std::string_view entry = text.substr(0, end);
        JunkMarks::Mark mark;
        if (ParseMark(entry, mark))
            set.Mark(mark.slot, mark.print);
        if (end == std::string_view::npos)
            break;
        text.remove_prefix(end + 1);
    }
    return set;
}
} // namespace GameLogic::Items::JunkMarks
