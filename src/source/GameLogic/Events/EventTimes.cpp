#include "stdafx.h"
#include "GameLogic/Events/EventTimes.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <cwchar>

namespace GameLogic::EventTimes
{
namespace
{
constexpr std::wstring_view Header = L"#EVT0|";
constexpr std::wstring_view Line = L"#EVT|";
constexpr std::wstring_view Footer = L"#EVT.|";
constexpr const wchar_t* RequestCommand = L"/eventtimes";
// Repeated requests within this time are dropped, so a window that opens and closes doesn't flood the server.
constexpr auto RequestInterval = std::chrono::seconds(3);

// Splits "a|b|c" into its fields.
std::vector<std::wstring> Split(std::wstring_view text)
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (true)
    {
        const size_t bar = text.find(L'|', start);
        fields.emplace_back(text.substr(start, bar == std::wstring_view::npos ? std::wstring_view::npos : bar - start));
        if (bar == std::wstring_view::npos)
            break;
        start = bar + 1;
    }
    return fields;
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
    if (view.starts_with(Header))
    {
        m_incoming.clear();
        m_receiving = true;
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_entries = std::move(m_incoming);
            m_incoming.clear();
            m_receiving = false;
            m_answered = true;
            m_hasTimes = true;
        }
        return true;
    }

    if (!view.starts_with(Line))
        return false;

    // "#EVT|phase|seconds|weekly|name": the name comes last, so it may hold anything but a line break.
    const auto fields = Split(view.substr(Line.size()));
    if (!m_receiving || fields.size() < 4)
        return true;

    Entry entry;
    entry.State = fields[0] == L"R" ? Phase::Running : fields[0] == L"O" ? Phase::EntranceOpen : Phase::Next;
    const long long seconds = std::wcstoll(fields[1].c_str(), nullptr, 10);
    entry.Weekly = fields[2] == L"1";
    entry.Name = fields[3];
    for (size_t i = 4; i < fields.size(); ++i)
        entry.Name += L"|" + fields[i];
    entry.Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    entry.WallDeadline = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
    m_incoming.push_back(std::move(entry));
    return true;
}

void Store::Request()
{
    const auto now = std::chrono::steady_clock::now();
    if (m_lastRequest != std::chrono::steady_clock::time_point{} && now - m_lastRequest < RequestInterval)
        return;
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    m_lastRequest = now;
    m_answered = false;
    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(RequestCommand));
}

void Store::Reset()
{
    m_entries.clear();
    m_incoming.clear();
    m_receiving = false;
    m_answered = false;
    m_hasTimes = false;
    m_lastRequest = {};
}
} // namespace GameLogic::EventTimes
