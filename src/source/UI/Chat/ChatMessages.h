#pragma once

#include "UI/Chat/MessageType.h"

#include <string_view>

namespace UI::Chat
{
void PostSystem(std::wstring_view text, mu::ui::window::MESSAGE_TYPE kind);
void PostChat(std::wstring_view sender, std::wstring_view text,
              mu::ui::window::MESSAGE_TYPE kind,
              mu::ui::window::MESSAGE_TYPE errorKind = mu::ui::window::TYPE_ALL_MESSAGE);

// The player turned incoming whispers off in the chat input.
bool IsWhisperBlocked();
// Someone whispered the player: their name joins the whisper-target history (Up/Down in the chat bar's name
// field), without filling the field.
void RememberWhisperSender(std::wstring_view sender);
void ResetLogFilter();
}
