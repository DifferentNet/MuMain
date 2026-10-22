#include "stdafx.h"
#include "GameLogic/Character/AdvancedStats.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <cwchar>

namespace GameLogic::AdvancedStats
{
namespace
{
constexpr std::wstring_view Prefix = L"#ADV|";
constexpr const wchar_t* Command = L"/advstats";
// The tab asks again after this time while it is open; requests in between are dropped.
constexpr auto RequestInterval = std::chrono::seconds(2);
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (!view.starts_with(Prefix))
        return false;

    // "key=value|key=value|..."
    std::map<std::wstring, double, std::less<>> values;
    size_t start = Prefix.size();
    while (start <= view.size())
    {
        const size_t bar = view.find(L'|', start);
        const std::wstring_view field = view.substr(start, bar == std::wstring_view::npos ? std::wstring_view::npos : bar - start);
        const size_t equals = field.find(L'=');
        if (equals != std::wstring_view::npos && equals > 0)
        {
            const std::wstring number(field.substr(equals + 1));
            values[std::wstring(field.substr(0, equals))] = std::wcstod(number.c_str(), nullptr);
        }
        if (bar == std::wstring_view::npos)
            break;
        start = bar + 1;
    }

    m_values = std::move(values);
    m_hasInfo = true;
    ++m_version;
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
    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(Command));
}

const double* Store::Find(std::wstring_view key) const
{
    const auto found = m_values.find(key);
    return found == m_values.end() ? nullptr : &found->second;
}

void Store::Reset()
{
    m_values.clear();
    m_hasInfo = false;
    ++m_version;
    m_lastRequest = {};
}
} // namespace GameLogic::AdvancedStats
