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
} // namespace UI::Chat::InviteAutoDecline
