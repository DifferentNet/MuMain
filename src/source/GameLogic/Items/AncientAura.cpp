#include "stdafx.h"
#include "GameLogic/Items/AncientAura.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

#include <cwchar>
#include <functional>
#include <unordered_map>

namespace GameLogic::AncientAura
{
namespace
{
constexpr std::wstring_view Line = L"#ANC|";
constexpr const wchar_t* Command = L"/ancaura";

struct NameHash
{
    using is_transparent = void;
    size_t operator()(std::wstring_view name) const noexcept
    {
        return std::hash<std::wstring_view>{}(name);
    }
};

// The characters with a tier, by name.
std::unordered_map<std::wstring, int, NameHash, std::equal_to<>> g_tiers;

bool IsHero(std::wstring_view name)
{
    return Hero != nullptr && name == std::wstring_view(Hero->ID);
}

// The own character's stats in the character window follow its tier.
void RecalculateOwnStats()
{
    if (CharacterMachine != nullptr && Hero != nullptr)
        CharacterMachine->CalculateAll();
}
} // namespace

bool TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (!view.starts_with(Line))
        return false;

    // "#ANC|name|tier"
    const std::wstring_view rest = view.substr(Line.size());
    const size_t bar = rest.find(L'|');
    if (bar == std::wstring_view::npos || bar == 0)
        return true;

    const std::wstring name(rest.substr(0, bar));
    const int value = static_cast<int>(std::wcstol(std::wstring(rest.substr(bar + 1)).c_str(), nullptr, 10));
    const int tier = value >= Plus15 ? Plus15 : value >= Plus14 ? Plus14 : None;

    const int before = TierOf(name);
    if (tier == None)
        g_tiers.erase(name);
    else
        g_tiers[name] = tier;

    if (tier != before && IsHero(name))
        RecalculateOwnStats();
    return true;
}

void Request()
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(Command));
}

void Reset()
{
    const bool hadOwn = OwnTier() != None;
    g_tiers.clear();
    if (hadOwn)
        RecalculateOwnStats();
}

int TierOf(std::wstring_view name)
{
    if (g_tiers.empty())
        return None;

    const auto it = g_tiers.find(name);
    return it == g_tiers.end() ? None : it->second;
}

int TierOf(const CHARACTER* c)
{
    if (c == nullptr || c->Object.Kind != KIND_PLAYER)
        return None;
    return TierOf(std::wstring_view(c->ID));
}

int OwnTier()
{
    return Hero == nullptr ? None : TierOf(std::wstring_view(Hero->ID));
}

namespace
{
// A new character: drop the last one's tiers and ask the server for the ones in the game now.
void ForgetAndAsk()
{
    Reset();
    Request();
}

// The tier lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"AncientAura",
    .prefixes = {Line},
    .take = [](const std::wstring& text) { return TakeMessage(text); },
    .enteredWorld = ForgetAndAsk,
});
} // namespace
} // namespace GameLogic::AncientAura
