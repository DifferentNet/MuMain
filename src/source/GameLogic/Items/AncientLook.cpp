#include "stdafx.h"
#include "GameLogic/Items/AncientLook.h"

#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Items/ExactLevelLine.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

#include <array>
#include <functional>
#include <unordered_map>

namespace GameLogic::AncientLook
{
namespace
{
constexpr std::wstring_view Line = L"#ANS|";
// "lv": the exact +14/+15 levels too (#UPL, GameLogic/Items/ExactLevelLine.h).
constexpr const wchar_t* Command = L"/ancset lv";
constexpr int HighestDiscriminator = 2;

using Discriminators = std::array<std::uint8_t, Parts>;

struct NameHash
{
    using is_transparent = void;
    size_t operator()(std::wstring_view name) const noexcept
    {
        return std::hash<std::wstring_view>{}(name);
    }
};

// The characters wearing ancient armour, by name.
std::unordered_map<std::wstring, Discriminators, NameHash, std::equal_to<>> g_sets;

// The characters wearing a +14 or +15, by name.
std::unordered_map<std::wstring, Items::ExactLevelLine::Levels, NameHash, std::equal_to<>> g_levels;

// "12021": a digit per part; anything else counts as 0. False when no part is ancient.
bool ReadDigits(std::wstring_view digits, Discriminators& parts)
{
    bool any = false;
    for (int i = 0; i < Parts; ++i)
    {
        const wchar_t digit = i < static_cast<int>(digits.size()) ? digits[static_cast<size_t>(i)] : L'0';
        const int value = digit - L'0';
        parts[static_cast<size_t>(i)] = static_cast<std::uint8_t>(value >= 1 && value <= HighestDiscriminator ? value : 0);
        any = any || parts[static_cast<size_t>(i)] != 0;
    }
    return any;
}

// A new character: drop the last one's sets and ask the server for the ones in the game now.
void ForgetAndAsk()
{
    Reset();
    Request();
}
} // namespace

bool TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(Items::ExactLevelLine::Prefix))
    {
        // "#UPL|name|e0f0000"
        if (auto line = Items::ExactLevelLine::Parse(view))
        {
            if (line->any)
                g_levels[line->name] = line->levels;
            else
                g_levels.erase(line->name);
        }
        return true;
    }
    if (!view.starts_with(Line))
        return false;

    // "#ANS|name|12021"
    const std::wstring_view rest = view.substr(Line.size());
    const size_t bar = rest.find(L'|');
    if (bar == std::wstring_view::npos || bar == 0)
        return true;

    Discriminators parts{};
    const std::wstring name(rest.substr(0, bar));
    if (ReadDigits(rest.substr(bar + 1), parts))
        g_sets[name] = parts;
    else
        g_sets.erase(name);
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
    g_sets.clear();
    g_levels.clear();
}

int DiscriminatorOf(std::wstring_view name, int part)
{
    if (g_sets.empty() || part < 0 || part >= Parts)
        return 0;

    const auto it = g_sets.find(name);
    return it == g_sets.end() ? 0 : it->second[static_cast<size_t>(part)];
}

int ExactLevelOf(std::wstring_view name, int piece)
{
    if (g_levels.empty() || piece < 0 || piece >= Items::ExactLevelLine::Pieces)
        return 0;

    const auto it = g_levels.find(name);
    return it == g_levels.end() ? 0 : it->second[static_cast<size_t>(piece)];
}

namespace
{
// The set lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"AncientLook",
    .prefixes = {Line, Items::ExactLevelLine::Prefix},
    .take = [](const std::wstring& text) { return TakeMessage(text); },
    .enteredWorld = ForgetAndAsk,
});
} // namespace
} // namespace GameLogic::AncientLook
