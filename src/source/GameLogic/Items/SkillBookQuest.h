#pragma once

// MU Moldova: the skill books that are read only after Marlon's quest "Gain Hero Status" (quest 2, from level 220).
// The client asks only what the server asks (OpenMU's item requirements): 12:44 Crystal of Destruction is the one
// book with that quest requirement. Nova 15:18, Wizardry Enhance 15:28, Multi-Shot 12:45 and Recovery 12:46 ask for
// class, level and energy only; the original client asked the quest for these four too, so a character the server
// lets learn them was told it could not.
namespace GameLogic::Items
{
bool SkillBookNeedsHeroStatusQuest(int itemType);
} // namespace GameLogic::Items
