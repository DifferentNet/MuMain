#include "stdafx.h"
#include "GameLogic/Character/GrandResetCount.h"

#include "GameLogic/Character/GrandResetCountLine.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

namespace GameLogic::GrandResetCount
{
namespace
{
constexpr const wchar_t* Command = L"/grsync";

int g_grandResets = 0;
} // namespace

bool TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (!view.starts_with(GrandResetCountLine::Prefix))
        return false;

    // A malformed line still stays out of the chat.
    if (const auto counts = GrandResetCountLine::Parse(view))
    {
        g_grandResets = counts->grandResets;
        // The server's resets: after a Grand Reset the level packet made them one more than before (101), this
        // puts them at 0 again.
        if (CharacterAttribute != nullptr)
            CharacterAttribute->Resets = static_cast<DWORD>(counts->resets);
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
    g_grandResets = 0;
}

int Own()
{
    return g_grandResets;
}

namespace
{
// A new character: drop the last Grand Resets and ask the server for the ones of the character in the game now.
void ForgetAndAsk()
{
    Reset();
    Request();
}

// The Grand Reset count line stays out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"GrandResetCount",
    .prefixes = {GrandResetCountLine::Prefix},
    .take = [](const std::wstring& text) { return TakeMessage(text); },
    .enteredWorld = ForgetAndAsk,
});
} // namespace
} // namespace GameLogic::GrandResetCount
