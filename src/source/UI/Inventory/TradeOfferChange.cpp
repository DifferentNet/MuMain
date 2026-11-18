// TradeOfferChange.cpp - which changes of the partner's offer the trade window says aloud.

#include "UI/Inventory/TradeOfferChange.h"

#include <utility>

namespace UI::Trade
{
bool IsZenLowered(std::int64_t before, std::int64_t after)
{
    return before > 0 && after < before;
}

void PendingRemovals::Hold(int type, int level, std::wstring line, std::uint32_t now)
{
    m_entries.push_back(Entry{ type, level, now, std::move(line) });
}

bool PendingRemovals::CancelOn(int type, int level)
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
    {
        if (it->type == type && it->level == level)
        {
            m_entries.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<std::wstring> PendingRemovals::TakeDue(std::uint32_t now)
{
    std::vector<std::wstring> due;
    auto it = m_entries.begin();
    while (it != m_entries.end())
    {
        if (now - it->at >= HoldMs)
        {
            due.push_back(std::move(it->line));
            it = m_entries.erase(it);
        }
        else
            ++it;
    }
    return due;
}
} // namespace UI::Trade
