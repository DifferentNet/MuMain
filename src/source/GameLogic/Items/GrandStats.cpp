#include "stdafx.h"
#include "GameLogic/Items/GrandStats.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

namespace GameLogic::GrandStats
{
namespace
{
constexpr const wchar_t* Command = L"/grstats sync";

Items::GrandStatsLine::Points g_points;

// The own character's stats in the character window follow its points.
void RecalculateOwnStats()
{
    if (CharacterMachine != nullptr && Hero != nullptr)
        CharacterMachine->CalculateAll();
}
} // namespace

bool TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (!view.starts_with(Items::GrandStatsLine::Prefix))
        return false;

    // A malformed line still stays out of the chat.
    if (const auto points = Items::GrandStatsLine::Parse(view); points && *points != g_points)
    {
        g_points = *points;
        RecalculateOwnStats();
    }
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
    const bool hadPoints = g_points != Items::GrandStatsLine::Points{};
    g_points = {};
    if (hadPoints)
        RecalculateOwnStats();
}

const Items::GrandStatsLine::Points& Own()
{
    return g_points;
}

namespace
{
// A new character (or map): drop the last points and ask the server for the ones of the character in the game now.
void ForgetAndAsk()
{
    Reset();
    Request();
}

// The Grand stats line stays out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"GrandStats",
    .prefixes = {Items::GrandStatsLine::Prefix},
    .take = [](const std::wstring& text) { return TakeMessage(text); },
    .enteredWorld = ForgetAndAsk,
});
} // namespace
} // namespace GameLogic::GrandStats
