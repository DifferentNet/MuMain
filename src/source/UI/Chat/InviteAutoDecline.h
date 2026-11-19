#pragma once

#include "UI/Chat/DeclinedInviteLines.h"

// "Decline invitations" in Options > Gameplay (config.ini [UI] DeclineInvitations, off by default).
namespace UI::Chat::InviteAutoDecline
{
// True when the option is on: the caller then sends the refusal its Cancel button sends and opens no
// dialog. Says the refusal in one chat line (one per sender and kind per DeclinedInviteLines::QuietMs).
[[nodiscard]] bool ShouldDecline(InviteKind kind, const wchar_t* sender);
// A trade request or duel challenge refused because a window was open: one chat line names who asked (one per
// sender and kind per DeclinedInviteLines::QuietMs), so the refusal is no longer silent. Party has none.
void SayRefusedWhileBusy(InviteKind kind, const wchar_t* sender);
} // namespace UI::Chat::InviteAutoDecline
