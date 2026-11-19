#include "stdafx.h"
#include "UI/Chat/InviteAutoDecline.h"

#include "Data/GameConfig/GameConfig.h"
#include "I18N/All.h"
#include "UI/Chat/ChatMessages.h"

namespace UI::Chat::InviteAutoDecline
{
namespace
{
DeclinedInviteLines g_lines;
DeclinedInviteLines g_busyLines;

const wchar_t* LineFormat(InviteKind kind)
{
    switch (kind)
    {
    case InviteKind::Party: return I18N::Game::InviteDeclinedParty;
    case InviteKind::Trade: return I18N::Game::InviteDeclinedTrade;
    case InviteKind::Duel: return I18N::Game::InviteDeclinedDuel;
    }
    return I18N::Game::InviteDeclinedParty;
}
} // namespace

bool ShouldDecline(InviteKind kind, const wchar_t* sender)
{
    if (!GameConfig::GetInstance().GetDeclineInvitations())
        return false;

    const wchar_t* name = sender != nullptr ? sender : L"";
    if (g_lines.ShouldSay(kind, name, GetTickCount()))
    {
        wchar_t line[256]{};
        mu_swprintf(line, LineFormat(kind), name);
        PostSystem(line, mu::ui::window::TYPE_SYSTEM_MESSAGE);
    }
    return true;
}

void SayRefusedWhileBusy(InviteKind kind, const wchar_t* sender)
{
    if (kind == InviteKind::Party)
        return;

    const wchar_t* name = sender != nullptr ? sender : L"";
    if (!g_busyLines.ShouldSay(kind, name, GetTickCount()))
        return;

    wchar_t line[256]{};
    const wchar_t* format = kind == InviteKind::Trade ? I18N::Game::InviteRefusedBusyTrade : I18N::Game::InviteRefusedBusyDuel;
    mu_swprintf(line, format, name);
    PostSystem(line, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}
} // namespace UI::Chat::InviteAutoDecline
