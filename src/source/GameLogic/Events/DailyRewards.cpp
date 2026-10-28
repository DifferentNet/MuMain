#include "stdafx.h"
#include "GameLogic/Events/DailyRewards.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

#include <cwchar>

namespace GameLogic::DailyRewards
{
namespace
{
constexpr std::wstring_view Header = L"#DLY0|";
constexpr std::wstring_view Line = L"#DLY|";
constexpr std::wstring_view Footer = L"#DLY.|";
constexpr const wchar_t* InfoCommand = L"/dailyinfo";
constexpr const wchar_t* TakeCommand = L"/daily";
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

int ToInt(const std::wstring& field)
{
    return static_cast<int>(std::wcstol(field.c_str(), nullptr, 10));
}

void SendCommand(const wchar_t* command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command));
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
        const auto fields = Split(view.substr(Header.size()));
        m_incomingDays.clear();
        m_incomingStatus = {};
        m_receiving = fields.size() >= 6;
        if (m_receiving)
        {
            m_incomingStatus.StreakDay = ToInt(fields[0]);
            m_incomingStatus.CountedToday = fields[1] == L"1";
            m_incomingStatus.SecondsToday = ToInt(fields[2]);
            m_incomingStatus.SecondsNeeded = ToInt(fields[3]);
            m_incomingStatus.Waiting = ToInt(fields[4]);
            m_incomingStatus.DaysInCycle = ToInt(fields[5]);
        }
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_status = m_incomingStatus;
            m_status.ReceivedAt = std::chrono::steady_clock::now();
            m_status.ReceivedWall = std::chrono::system_clock::now();
            m_days = std::move(m_incomingDays);
            m_incomingDays.clear();
            m_receiving = false;
            m_hasInfo = true;
        }
        return true;
    }

    if (!view.starts_with(Line))
        return false;

    // "#DLY|day|reward": the reward comes last, so it may hold anything but a line break.
    const auto fields = Split(view.substr(Line.size()));
    if (!m_receiving || fields.size() < 2)
        return true;

    Day day;
    day.Number = ToInt(fields[0]);
    day.Reward = fields[1];
    for (size_t i = 2; i < fields.size(); ++i)
        day.Reward += L"|" + fields[i];
    m_incomingDays.push_back(std::move(day));
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
    SendCommand(InfoCommand);
}

void Store::TakeWaiting()
{
    SendCommand(TakeCommand);
}

void Store::Reset()
{
    m_status = {};
    m_days.clear();
    m_incomingStatus = {};
    m_incomingDays.clear();
    m_receiving = false;
    m_hasInfo = false;
    m_lastRequest = {};
}

namespace
{
// The daily rewards window's lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"DailyRewards",
    .prefixes = {Header, Line, Footer},
    .take = [](const std::wstring& text) { return Rewards().TakeMessage(text); },
    .enteredWorld = [] { Rewards().Reset(); },
});
} // namespace
} // namespace GameLogic::DailyRewards
