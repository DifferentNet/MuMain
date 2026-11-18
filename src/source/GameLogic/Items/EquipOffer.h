#pragma once

#include <cstdint>

// The quick equip offer (pad round 6, B3; ghinzuro 2026-11-18, pad and touch only): when a picked-up item would raise
// the hero's Combat Power and the hero may wear it, a short offer shows it ("New: +N CP"); the player puts it on by
// holding the d-pad right or tapping the offer. It never puts anything on by itself. This file holds the pure rules;
// UI/Inventory/QuickEquip reads the items and shows the offer.
namespace GameLogic::Items::EquipOffer
{
// How long an offer stays up.
inline constexpr std::uint32_t LifetimeMs = 10'000;
// A tap this soon after an offer appeared (or a better one took its place) is not taken: the finger was already on
// its way to something else under the chip.
inline constexpr std::uint32_t TapGuardMs = 300;

// A picked-up item that might be offered.
struct Candidate
{
    int bagIndex = -1;           // the protocol's slot of the bag cell it went into
    std::uint32_t itemKey = 0;   // the client's item key, to tell it is still the same item
    int combatPowerChange = 0;   // the item tooltip's CP change (GameLogic::Items::GetHeroCombatPowerChange)
    bool canWear = false;        // class, level and stats allow it
    bool canPutOnNow = false;    // the slot is free, or one worn item would come off to make room
};

// Only an item the hero may wear, that one move can put on, and that adds Combat Power.
bool ShouldOffer(const Candidate& candidate);

// The offer on show.
struct Offer
{
    int bagIndex = -1;
    std::uint32_t itemKey = 0;
    int combatPowerChange = 0;
    std::uint32_t shownAt = 0;
};

// Whether a finger's tap at now may take offer (TapGuardMs).
bool TakesTap(const Offer& offer, std::uint32_t now);
} // namespace GameLogic::Items::EquipOffer
