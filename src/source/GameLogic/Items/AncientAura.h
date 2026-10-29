#pragma once

#include <string>
#include <string_view>

class CHARACTER;

// The +14/+15 ancient tier (MU Skynet, ghinzuro 2026-10-29): a complete ancient set whose weapons, shields and
// armour are all +14 or higher raises strength, agility, vitality, energy and command by 5 %, all +15 by 10 %
// (pendants and rings are needed for the set, their level doesn't count). Its character's full-set glow
// (RenderCharacter in Engine/Object/ZzzCharacter.cpp) is then drawn in gold, also when its armour pieces aren't all
// of one kind, and gold wins over the set's own colour.
//
// Our server's add-on (MU Moldova, AncientAura) works the tier out and answers the hidden chat command "/ancaura",
// which this sends once the character entered the world, with a blue server message per character in the game
// with a tier; from then on it sends one for a character with a tier that enters the world, for a change and a 0
// for one that leaves the world. This takes them out of the chat:
//   "#ANC|name|tier"   (tier 0, 14 or 15)
// The own character's line also adds the stats to the character window (CHARACTER_MACHINE::CalculateAll), as the
// server multiplies the total stats.
namespace GameLogic::AncientAura
{
constexpr int None = 0;
constexpr int Plus14 = 14;
constexpr int Plus15 = 15;

// The stats bonus of a tier in percent: 5 for +14, 10 for +15, else 0.
constexpr int StatPercent(int tier)
{
    return tier == Plus15 ? 10 : tier == Plus14 ? 5 : 0;
}

// What a total stat (the base and every item's bonus) gains from a tier: the server's factor 1.05 / 1.10, rounded
// down as the character window shows the server's whole stats.
constexpr int StatBonus(int total, int tier)
{
    return total > 0 ? total * StatPercent(tier) / 100 : 0;
}

// Takes a server message. True when it was a tier line, which then stays out of the chat.
bool TakeMessage(const std::wstring& text);

// Asks the server for the tiers of the characters in the game ("/ancaura").
void Request();

// Forgets the tiers, e.g. when the character changed; the next Request() brings them again.
void Reset();

// A character's tier by its name as the game shows it: None, Plus14 or Plus15.
int TierOf(std::wstring_view name);

// A character's tier (players only; None for monsters and NPCs).
int TierOf(const CHARACTER* c);

// The own character's tier.
int OwnTier();
} // namespace GameLogic::AncientAura
