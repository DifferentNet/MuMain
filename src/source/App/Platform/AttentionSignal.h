#pragma once

// Calls the player back to a game window that is not in front when something waits for an answer: a whisper, or a
// party, trade, guild or duel request (Network/Server/WSclient.cpp). Never for normal chat.
namespace App::Platform::Attention
{
// PC: the taskbar button flashes until the window gets the focus. /play: the tab title starts with "(!) " until the
// page gets the focus back. Nothing when the window is already in front.
void Request();
} // namespace App::Platform::Attention
