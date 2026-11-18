#include "GameLogic/Items/CraftingRefusal.h"

namespace GameLogic::Items::CraftingRefusal
{
Reason ReasonOf(int resultCode, int mixId)
{
    switch (resultCode)
    {
    case ResultTooManyItems:
        return Reason::TooManyItems;
    case ResultCharacterLevelTooLow:
        return mixId == DevilSquareTicketMixId ? Reason::DevilSquareLevel : Reason::LevelTooLow;
    case ResultLackingMixItems:
        return Reason::ItemsMissing;
    case ResultIncorrectMixItems:
    case ResultIncorrectBloodCastleItems:
        return Reason::ImproperItems;
    case ResultInvalidItemLevel:
        return Reason::ItemLevel;
    case ResultCharacterClassTooLow:
        return mixId == BloodCastleTicketMixId ? Reason::BloodCastleLevel : Reason::ClassCannot;
    default:
        return Reason::None;
    }
}
} // namespace GameLogic::Items::CraftingRefusal
