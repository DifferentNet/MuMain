#include "GameLogic/Items/EquipOfferTracker.h"

namespace GameLogic::Items::EquipOffer
{
bool EquipOfferTracker::Consider(const Candidate& candidate, std::uint32_t now)
{
    if (!ShouldOffer(candidate))
        return false;
    const Offer* current = Current(now);
    if (current != nullptr && current->bagIndex != candidate.bagIndex &&
        current->combatPowerChange >= candidate.combatPowerChange)
        return false;
    m_offer = {candidate.bagIndex, candidate.itemKey, candidate.combatPowerChange, now};
    m_shown = true;
    return true;
}

const Offer* EquipOfferTracker::Current(std::uint32_t now) const
{
    if (!m_shown || now - m_offer.shownAt >= LifetimeMs)
        return nullptr;
    return &m_offer;
}

void EquipOfferTracker::ItemLeft(int bagIndex)
{
    if (m_shown && m_offer.bagIndex == bagIndex)
        m_shown = false;
}

void EquipOfferTracker::Clear()
{
    m_shown = false;
}
} // namespace GameLogic::Items::EquipOffer
