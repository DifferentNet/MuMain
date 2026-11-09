#pragma once

#include <string>
#include <string_view>

// Which ancient set other characters wear (MU Skynet, ghinzuro 2026-11-07): the appearance of a character only says
// whether a piece is ancient, and two ancient sets share each armour model (Kantata and Rave on Plate), so the
// client couldn't tell which colour to draw (Render/Items/AncientColors.h).
//
// Our server's add-on (MU Moldova, AncientLook) answers the hidden chat command "/ancset", which this sends once the
// character entered the world, with a blue server message per character in the game wearing ancient armour; from then
// on it sends one for such a character entering the world, for a change and "00000" for one that leaves. This takes
// them out of the chat:
//   "#ANS|name|12021"   (the ancient discriminator of the helm, armour, pants, gloves and boots: 0, 1 or 2)
// Asked "/ancset lv", it also sends the exact +14/+15 of the worn pieces (GameLogic/Items/ExactLevelLine.h):
//   "#UPL|name|e0f0000"
namespace GameLogic::AncientLook
{
// Helm, armour, pants, gloves, boots.
constexpr int Parts = 5;

// Takes a server message. True when it was a set line, which then stays out of the chat.
bool TakeMessage(const std::wstring& text);

// Asks the server for the sets of the characters in the game ("/ancset").
void Request();

// Forgets the sets, e.g. when the character changed; the next Request() brings them again.
void Reset();

// The ancient discriminator (1 or 2) of a character's piece by its name as the game shows it; part 0 is the helm,
// 4 the boots. 0 when the piece isn't ancient or the server didn't say yet.
int DiscriminatorOf(std::wstring_view name, int part);

// The exact level (14 or 15) of a character's worn piece by its name: piece 0 and 1 are the left and right hand,
// 2 to 6 the helm to the boots (ExactLevelLine::Pieces). 0 when it is lower, empty or the server didn't say.
int ExactLevelOf(std::wstring_view name, int piece);
} // namespace GameLogic::AncientLook
