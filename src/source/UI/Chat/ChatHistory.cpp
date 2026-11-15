#include "UI/Chat/ChatHistory.h"

#include <algorithm>

namespace UI::Chat
{
void ChatHistory::Add(std::wstring_view line)
{
    ResetBrowsing();
    if (line.empty())
        return;

    const auto same = std::find(m_lines.begin(), m_lines.end(), line);
    if (same != m_lines.end())
        m_lines.erase(same);
    else if (m_lines.size() >= kCapacity)
        m_lines.erase(m_lines.begin());
    m_lines.emplace_back(line);
    m_cursor = m_lines.size();
}

void ChatHistory::Remove(std::size_t index)
{
    if (index < m_lines.size())
        m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(index));
    ResetBrowsing();
}

void ChatHistory::Clear()
{
    m_lines.clear();
    ResetBrowsing();
}

void ChatHistory::ResetBrowsing()
{
    m_browsing = false;
    m_draft.clear();
    m_cursor = m_lines.size();
}

std::optional<std::wstring> ChatHistory::Older(std::wstring_view draft)
{
    if (m_lines.empty())
        return std::nullopt;

    if (!m_browsing)
    {
        m_browsing = true;
        m_draft.assign(draft);
        m_cursor = m_lines.size();
    }
    if (m_cursor > 0)
        --m_cursor;
    return m_lines[m_cursor];
}

std::optional<std::wstring> ChatHistory::Newer()
{
    if (!m_browsing)
        return std::nullopt;

    if (m_cursor + 1 < m_lines.size())
    {
        ++m_cursor;
        return m_lines[m_cursor];
    }
    std::wstring draft = std::move(m_draft);
    ResetBrowsing();
    return draft;
}
} // namespace UI::Chat
