#pragma once

#include <cstdint>

// Whether the character has the mana and AG (ability gauge) a skill spends, the same comparison the attack code makes
// before it casts (GameLogic/Combat/SkillCast.cpp, ClassAttack.cpp). The skill bar greys a skill that fails it.
namespace GameLogic::Skills
{
bool HasResourcesFor(std::uint32_t mana, std::uint32_t ag, int manaCost, int agCost);
} // namespace GameLogic::Skills
