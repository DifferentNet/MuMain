#include "stdafx.h"

#include "UI/Chat/ChatMessages.h"

#include "UI/Chat/ItemLinks.h"
#include "UI/Core/WindowSystem.h"

#include <utility>

namespace UI::Chat
{
void PostSystem(std::wstring_view text, mu::ui::window::MESSAGE_TYPE kind)
{
    g_pSystemLogBox->AddText(std::wstring(text), kind);
}

void PostChat(std::wstring_view sender, std::wstring_view text,
              mu::ui::window::MESSAGE_TYPE kind, mu::ui::window::MESSAGE_TYPE errorKind)
{
    // A line the server announced items for shows them as links (UI/Chat/ItemLinks.h).
    if (auto linked = ItemLinks::TakeFor(sender, text, kind))
    {
        g_pChatListBox->AddLinkedText(std::wstring(sender), std::move(linked->text), kind, std::move(linked->links));
        return;
    }
    g_pChatListBox->AddText(std::wstring(sender), std::wstring(text), kind, errorKind);
}

bool IsWhisperBlocked()
{
    return g_pChatInputBox->IsBlockWhisper();
}

void RememberWhisperSender(std::wstring_view sender)
{
    if (sender.empty() || g_pChatInputBox == nullptr)
        return;
    g_pChatInputBox->AddWhsprIDHistory(std::wstring(sender));
    g_pChatInputBox->SetLastWhisperSender(std::wstring(sender));
}

void ResetLogFilter()
{
    g_pChatListBox->ResetFilter();
}
}
