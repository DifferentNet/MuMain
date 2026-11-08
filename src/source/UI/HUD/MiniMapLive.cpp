#include "stdafx.h"
#include "UI/HUD/MiniMapLive.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

#include <algorithm>
#include <cwchar>
#include <string_view>

namespace UI::MiniMapLive
{
namespace
{
constexpr std::wstring_view StartTag = L"#MML0|";
constexpr std::wstring_view RowTag = L"#MML|";
constexpr std::wstring_view EndTag = L"#MML.|";
constexpr const wchar_t* Command = L"/mmlive";
constexpr auto RequestInterval = std::chrono::seconds(5);
// A list longer than the server ever sends (20 rows) is cut here.
constexpr size_t MaxEntries = 32;

// The fields after the tag, split at '|'; the last one keeps any further bars.
std::vector<std::wstring_view> Fields(std::wstring_view rest, size_t count)
{
    std::vector<std::wstring_view> fields;
    while (fields.size() + 1 < count)
    {
        const size_t bar = rest.find(L'|');
        if (bar == std::wstring_view::npos)
            break;
        fields.push_back(rest.substr(0, bar));
        rest.remove_prefix(bar + 1);
    }
    fields.push_back(rest);
    return fields;
}

int Number(std::wstring_view field)
{
    const std::wstring text(field);
    return static_cast<int>(std::wcstol(text.c_str(), nullptr, 10));
}
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(StartTag))
    {
        const auto fields = Fields(view.substr(StartTag.size()), 2);
        m_pending.clear();
        m_pendingMap = Number(fields[0]);
        m_pendingCell = fields.size() > 1 ? Number(fields[1]) : 16;
        if (m_pendingCell <= 0 || m_pendingCell > 64)
            m_pendingCell = 16;
        m_receiving = true;
        return true;
    }
    if (view.starts_with(EndTag))
    {
        if (m_receiving)
        {
            m_entries = std::move(m_pending);
            m_pending.clear();
            m_map = m_pendingMap;
            m_cell = m_pendingCell;
            m_receiving = false;
            ++m_version;
        }
        return true;
    }
    if (view.starts_with(RowTag))
    {
        const auto fields = Fields(view.substr(RowTag.size()), 5);
        if (m_receiving && fields.size() == 5 && fields[0].size() == 1 && m_pending.size() < MaxEntries)
        {
            Entry entry;
            entry.kind = fields[0][0];
            entry.x = std::clamp(Number(fields[1]), 0, 255);
            entry.y = std::clamp(Number(fields[2]), 0, 255);
            entry.count = std::max(1, Number(fields[3]));
            entry.name = std::wstring(fields[4]);
            m_pending.push_back(std::move(entry));
        }
        return true;
    }
    return false;
}

void Store::Request()
{
    const auto now = std::chrono::steady_clock::now();
    if (m_lastRequest != std::chrono::steady_clock::time_point{} && now - m_lastRequest < RequestInterval)
        return;
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    m_lastRequest = now;
    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(Command));
}

void Store::Reset()
{
    if (!m_entries.empty())
        ++m_version;
    m_entries.clear();
    m_pending.clear();
    m_map = m_pendingMap = -1;
    m_receiving = false;
    m_lastRequest = {};
}

const std::vector<Entry>* Store::EntriesFor(int map) const
{
    return map == m_map ? &m_entries : nullptr;
}

namespace
{
// The map's live monster lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"MiniMapLive",
    .prefixes = {StartTag, RowTag, EndTag},
    .take = [](const std::wstring& text) { return Live().TakeMessage(text); },
    .enteredWorld = [] { Live().Reset(); },
});
} // namespace
} // namespace UI::MiniMapLive
