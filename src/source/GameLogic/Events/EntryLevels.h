#pragma once

#include <string>

// The level bands of Blood Castle and Devil Square: the entry windows (UI/Events/BloodCastleEnter.cpp,
// EnterDevilSquare.cpp) and the tooltips of their tickets (Engine/Object/ZzzInventory.cpp) read the same table.
// The server's MiniGameDefinition asks the same levels (trim/bc8-ds7-third-class.sql).
namespace GameLogic::Events::EntryLevels
{
enum class Event
{
    BloodCastle, // castles 1-8, ticket: Cloak of Invisibility +1..+8
    DevilSquare, // squares 1-7, ticket: Devil's Invitation +1..+7
};

struct Band
{
    int minLevel = 0;
    int maxLevel = 0;
};

// 8 castles or 7 squares; the last one is the third class's.
[[nodiscard]] int GradeCount(Event event);

// The band of a grade (0-based). Magic Gladiators, Dark Lords and Rage Fighters ("strong" classes) start lower in
// Blood Castle and higher in Devil Square, as in the original. A grade out of range gives an empty band.
[[nodiscard]] Band BandOf(Event event, bool strongClass, int grade);

// The grade (0-based) a hero of this level enters, or -1 when no band fits (below the first one). A third class
// goes to the last grade from its band's level on; below it (after a reset) it takes the grade of its level.
[[nodiscard]] int GradeFor(Event event, bool strongClass, bool thirdClass, int level);

// For the hero (EntryLevelsHero.cpp, needs the game state).
// Whether the hero's class reads the "strong" table (Magic Gladiator, Dark Lord, Rage Fighter).
[[nodiscard]] bool HeroIsStrongClass();
// GradeFor() with the hero's class and level: -1 when no band fits.
[[nodiscard]] int HeroGrade(Event event);
// "Castle 3 (level 131-180)" / "Square 7 (level 331-400, 3rd class)" in the hero's class's table.
[[nodiscard]] std::wstring BandLabel(Event event, int grade);
} // namespace GameLogic::Events::EntryLevels
