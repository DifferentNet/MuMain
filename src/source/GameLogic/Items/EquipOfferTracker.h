#pragma once

#include "GameLogic/Items/EquipOffer.h"

namespace GameLogic::Items::EquipOffer
{
// Keeps at most one offer: the best of the recent pick-ups, until it runs out, is taken or its item leaves the bag.
class EquipOfferTracker
{
public:
    // Shows candidate when it should be offered and beats the offer on show (or none is up); true if it is shown.
    bool Consider(const Candidate& candidate, std::uint32_t now);
    // The offer on show, or nullptr once it ran out or was cleared.
    const Offer* Current(std::uint32_t now) const;
    // The item at bagIndex left its cell (sold, dropped, moved, put on): its offer goes.
    void ItemLeft(int bagIndex);
    void Clear();

private:
    Offer m_offer;
    bool m_shown = false;
};
} // namespace GameLogic::Items::EquipOffer
