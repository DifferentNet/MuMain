#include "Core/Utilities/RecentNames.h"

#include <algorithm>

namespace Core::Utilities
{
RecentNames::RecentNames(std::size_t capacity) : m_capacity(capacity)
{
}

bool RecentNames::Note(std::string_view name)
{
    if (name.empty() || m_capacity == 0)
    {
        return false;
    }
    if (!m_names.empty() && m_names.back() == name)
    {
        return false;
    }
    const auto found = std::find(m_names.begin(), m_names.end(), name);
    if (found != m_names.end())
    {
        m_names.erase(found);
    }
    m_names.emplace_back(name);
    if (m_names.size() > m_capacity)
    {
        m_names.erase(m_names.begin(), m_names.begin() + static_cast<std::ptrdiff_t>(m_names.size() - m_capacity));
    }
    return true;
}

std::string RecentNames::Join() const
{
    std::string text;
    for (const std::string& name : m_names)
    {
        text += name;
        text += '\n';
    }
    return text;
}

void RecentNames::Load(std::string_view text)
{
    m_names.clear();
    while (!text.empty())
    {
        const std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        Note(line);
        if (end == std::string_view::npos)
        {
            break;
        }
        text.remove_prefix(end + 1);
    }
}
} // namespace Core::Utilities
