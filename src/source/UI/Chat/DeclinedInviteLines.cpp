#include "UI/Chat/DeclinedInviteLines.h"

#include <algorithm>

namespace UI::Chat
{
bool DeclinedInviteLines::ShouldSay(InviteKind kind, std::wstring_view sender, std::uint32_t nowMs)
{
    const auto same = std::find_if(m_said.begin(), m_said.end(),
        [&](const Said& said) { return said.kind == kind && said.sender == sender; });
    if (same != m_said.end())
    {
        // Unsigned subtraction keeps the gap right across the tick counter's wrap.
        if (nowMs - same->at < QuietMs)
            return false;
        same->at = nowMs;
        return true;
    }

    if (m_said.size() >= MaxRemembered)
    {
        const auto oldest = std::min_element(m_said.begin(), m_said.end(),
            [nowMs](const Said& a, const Said& b) { return nowMs - a.at > nowMs - b.at; });
        m_said.erase(oldest);
    }
    m_said.push_back({ kind, std::wstring(sender), nowMs });
    return true;
}
} // namespace UI::Chat
