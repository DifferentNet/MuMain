#pragma once

#include <string>

// The own character's Grand Resets for the character window (MU Skynet, ghinzuro 2026-11-22: «пишет 101 рес, хотя это
// по факту 1 ГР», format «GR перед ресетами»): with Grand Resets the window says "Level: 375 | GR: 1 | Resets: 0",
// without them "Level: 375 | Resets: 87" as before.
//
// The server keeps the Grand Resets apart from the resets, which a Grand Reset sets back to 0. The client knows only
// the resets (character information, F3 03), and its level packet handler (WSclient.cpp ReceiveLevelUp) adds 1 to
// them when the level drops, a Grand Reset too. Our server's add-on (MU Moldova, GrandReset) answers the hidden chat
// command "/grsync", which this sends once the character entered the world, and from then on sends it again right
// after every Grand Reset (after the level packet). This takes the line out of the chat:
//   "#GRC|grandResets|resets"   (GameLogic/Character/GrandResetCountLine.h)
// keeps the Grand Resets and sets the window's resets to the server's. A server without the command never answers:
// then nothing changes.
namespace GameLogic::GrandResetCount
{
// Takes a server message. True when it was a Grand Reset count line, which then stays out of the chat.
bool TakeMessage(const std::wstring& text);

// Asks the server for the own character's counts ("/grsync").
void Request();

// Forgets the Grand Resets, e.g. when the character changed; the next Request() brings them again.
void Reset();

// The own character's Grand Resets (0 until the server sent them, or on a server without them).
int Own();
} // namespace GameLogic::GrandResetCount
