#pragma once

// Why the server refused a chaos machine combination (its CraftingResult codes). The client's own recipe check does
// not know the server-only recipes (Excellent Dust, Box +5 excellent, bag pages), so without this the button just
// does nothing. The two old message boxes stay for the recipes they were written for: the Devil Square invitation's
// level and the Blood Castle cloak's level.
namespace GameLogic::Items::CraftingRefusal
{
// The server's result codes that refuse a combination.
inline constexpr int ResultTooManyItems = 3;
inline constexpr int ResultCharacterLevelTooLow = 4;
inline constexpr int ResultLackingMixItems = 6;
inline constexpr int ResultIncorrectMixItems = 7;
inline constexpr int ResultInvalidItemLevel = 8;
inline constexpr int ResultCharacterClassTooLow = 9;
inline constexpr int ResultIncorrectBloodCastleItems = 10;

// Recipe numbers (Mix.bmd's m_iMixID, the server's ItemCrafting numbers) of the ticket recipes.
inline constexpr int DevilSquareTicketMixId = 2;
inline constexpr int BloodCastleTicketMixId = 8;

enum class Reason
{
    None,               // not a refusal this explains
    TooManyItems,
    ItemsMissing,
    ImproperItems,
    ItemLevel,
    LevelTooLow,
    ClassCannot,
    DevilSquareLevel,   // the old box: level 10 for the Devil Square invitation
    BloodCastleLevel,   // the old box: level 15 for the Cloak of Invisibility
};

// The reason for a result code, given the recipe the machine's items make (mixId; anything else when none).
[[nodiscard]] Reason ReasonOf(int resultCode, int mixId);
} // namespace GameLogic::Items::CraftingRefusal
