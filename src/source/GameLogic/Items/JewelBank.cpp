#include "stdafx.h"
#include "GameLogic/Items/JewelBank.h"

#include "Data/GameData/ItemData/ItemType.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <cwchar>
#include <string_view>

namespace GameLogic::JewelBank
{
namespace
{
constexpr std::wstring_view Answer = L"#JBK|";
constexpr std::wstring_view Unavailable = L"#JBK!|";
// Repeated requests within this time are dropped, so a window that opens and closes doesn't flood the server.
constexpr auto RequestInterval = std::chrono::seconds(1);

void SendCommand(const std::wstring& command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command.c_str()));
}

constexpr Kind Make(int group, int number, int packedNumber)
{
    return {Data::Items::MakeItemType(group, number), Data::Items::MakeItemType(12, packedNumber)};
}
} // namespace

const std::array<Kind, KindCount>& Kinds()
{
    // Chaos, Life, Soul, Bless, Creation, Guardian, Harmony, Gemstone, Lower and Higher Refine Stone.
    static constexpr std::array<Kind, KindCount> kinds{
        Make(12, 15, 141), Make(14, 16, 136), Make(14, 14, 31), Make(14, 13, 30), Make(14, 22, 137),
        Make(14, 31, 138), Make(14, 42, 140), Make(14, 41, 139), Make(14, 43, 142), Make(14, 44, 143),
    };
    return kinds;
}

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(Unavailable))
    {
        m_unavailable = true;
        m_hasInfo = false;
        return true;
    }

    if (!view.starts_with(Answer))
        return false;

    std::array<int, KindCount> counts{};
    size_t start = Answer.size();
    for (int i = 0; i < KindCount; ++i)
    {
        if (start > view.size())
            return true; // too few fields: not ours to show
        const size_t bar = view.find(L'|', start);
        const std::wstring field(view.substr(start, bar == std::wstring_view::npos ? std::wstring_view::npos : bar - start));
        counts[i] = static_cast<int>(std::wcstol(field.c_str(), nullptr, 10));
        start = bar == std::wstring_view::npos ? view.size() + 1 : bar + 1;
    }

    m_counts = counts;
    m_hasInfo = true;
    m_unavailable = false;
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
    SendCommand(L"/jbinfo");
}

void Store::Deposit(int kind)
{
    if (kind < 0 || kind >= KindCount)
        return;
    SendCommand(L"/jbdep " + std::to_wstring(kind + 1));
}

void Store::Withdraw(int kind, int amount)
{
    if (kind < 0 || kind >= KindCount || amount < 1)
        return;
    SendCommand(L"/jbwd " + std::to_wstring(kind + 1) + L" " + std::to_wstring(amount));
}

void Store::Reset()
{
    m_counts = {};
    m_hasInfo = false;
    m_unavailable = false;
    m_lastRequest = {};
}
} // namespace GameLogic::JewelBank
