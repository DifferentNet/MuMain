#include "GameLogic/Items/EquipOffer.h"

namespace GameLogic::Items::EquipOffer
{
bool ShouldOffer(const Candidate& candidate)
{
    return candidate.bagIndex >= 0 && candidate.canWear && candidate.canPutOnNow && candidate.combatPowerChange > 0;
}

bool TakesTap(const Offer& offer, std::uint32_t now)
{
    return now - offer.shownAt >= TapGuardMs;
}
} // namespace GameLogic::Items::EquipOffer
