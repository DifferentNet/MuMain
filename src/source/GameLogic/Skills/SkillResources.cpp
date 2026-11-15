#include "GameLogic/Skills/SkillResources.h"

namespace GameLogic::Skills
{
bool HasResourcesFor(std::uint32_t mana, std::uint32_t ag, int manaCost, int agCost)
{
    const bool enoughMana = manaCost <= 0 || mana >= static_cast<std::uint32_t>(manaCost);
    const bool enoughAg = agCost <= 0 || ag >= static_cast<std::uint32_t>(agCost);
    return enoughMana && enoughAg;
}
} // namespace GameLogic::Skills
