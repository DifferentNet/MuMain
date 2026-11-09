#include "stdafx.h"
#include "GameLogic/Automation/TargetPriority.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

namespace GameLogic::Automation
{
namespace
{
constexpr const wchar_t* Command = L"/mst";
std::chrono::steady_clock::time_point s_lastRequest{};
} // namespace

MonsterStrengths& Strengths()
{
    static MonsterStrengths strengths;
    return strengths;
}

void RequestStrengths()
{
    const auto now = std::chrono::steady_clock::now();
    if (s_lastRequest != std::chrono::steady_clock::time_point{} && now - s_lastRequest < StrengthRequestInterval)
        return;
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    s_lastRequest = now;
    // Sent as a chat line, as typed commands are; the server's add-on answers with hidden #MST lines.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(Command));
}

namespace
{
// The /mst answer stays out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"HelperTargets",
    .prefixes = {StrengthListStart, StrengthListMore},
    .take = [](const std::wstring& text) { return Strengths().Take(text, std::chrono::steady_clock::now()); },
    .enteredWorld =
        [] {
            Strengths().Clear();
            s_lastRequest = {};
        },
});
} // namespace
} // namespace GameLogic::Automation
