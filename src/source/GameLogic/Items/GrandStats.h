#pragma once

#include "GameLogic/Items/GrandStatsLine.h"

#include <string>

// The Grand stat points (MU Skynet, ghinzuro 2026-11-12: "5000, through /grstats"): every Grand Reset gives a character
// stat points it puts into strength, agility, vitality, energy and (Dark Lord) command with /grstats, for good. The
// server adds them to the total stats; the character window shows the same.
//
// Our server's add-on (MU Moldova, GrandStats) answers the hidden chat command "/grstats sync", which this sends once
// the character entered the world, with the own character's points, and from then on sends them again after every
// change and every Grand Reset. This takes the line out of the chat:
//   "#GRS|str|agi|vit|ene|cmd"   (GameLogic/Items/GrandStatsLine.h)
// and adds the points to the character window (CHARACTER_MACHINE::CalculateAll, before the +14/+15 ancient tier,
// which the server multiplies them by too).
namespace GameLogic::GrandStats
{
// Takes a server message. True when it was a Grand stats line, which then stays out of the chat.
bool TakeMessage(const std::wstring& text);

// Asks the server for the own character's points ("/grstats sync").
void Request();

// Forgets the points, e.g. when the character changed; the next Request() brings them again.
void Reset();

// The own character's points (all 0 until the server sent them, or on a server without them).
const Items::GrandStatsLine::Points& Own();
} // namespace GameLogic::GrandStats
