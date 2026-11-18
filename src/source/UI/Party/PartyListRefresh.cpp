#include "stdafx.h"
#include "UI/Party/PartyListRefresh.h"

#include "Dotnet/Connection.h"
#include "Network/Server/WSclient.h"

namespace UI::Party::ListRefresh
{
namespace
{
constexpr std::uint32_t IntervalMs = 5'000;

bool g_asked = false;
std::uint32_t g_askedAt = 0;
} // namespace

void Tick(bool wanted)
{
    const std::uint32_t now = GetTickCount();
    if (!RefreshDue(wanted, g_asked, g_askedAt, now, IntervalMs))
        return;
    if (SocketClient == nullptr)
        return;

    SocketClient->ToGameServer()->SendPartyListRequest();
    NoteAsked();
}

void NoteAsked()
{
    g_asked = true;
    g_askedAt = GetTickCount();
}
} // namespace UI::Party::ListRefresh
