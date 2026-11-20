#include "UI/Chat/ItemLinkPending.h"

#include <algorithm>

namespace UI::Chat
{
void ItemLinkPending::Put(ItemLinkWire::LinksLine line, std::uint64_t nowMs)
{
    DropExpired(nowMs);
    std::erase_if(m_entries, [&line](const Entry& entry)
                  { return entry.line.channel == line.channel && entry.line.sender == line.sender; });
    if (m_entries.size() >= MaxWaiting)
        m_entries.erase(m_entries.begin());
    m_entries.push_back({std::move(line), nowMs});
}

std::optional<ItemLinkWire::LinksLine> ItemLinkPending::Take(std::wstring_view sender, wchar_t channel, std::uint64_t nowMs)
{
    if (m_entries.empty())
        return std::nullopt;
    DropExpired(nowMs);
    const auto found = std::find_if(m_entries.begin(), m_entries.end(), [sender, channel](const Entry& entry)
                                    { return entry.line.channel == channel && entry.line.sender == sender; });
    if (found == m_entries.end())
        return std::nullopt;
    ItemLinkWire::LinksLine line = std::move(found->line);
    m_entries.erase(found);
    return line;
}

void ItemLinkPending::Clear()
{
    m_entries.clear();
}

void ItemLinkPending::DropExpired(std::uint64_t nowMs)
{
    std::erase_if(m_entries, [nowMs](const Entry& entry)
                  { return nowMs < entry.arrivedMs || nowMs - entry.arrivedMs > KeptMs; });
}
} // namespace UI::Chat
