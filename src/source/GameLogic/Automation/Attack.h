// Mouse-free plain attacks.
//
// Extracted from MUHelper/MuHelper.cpp. The auto-helper calls it with players
// excluded (it is a grinding aid); the control socket allows them, because a
// PvP scenario is exactly what it is for. Everything else — range by weapon,
// walking into range two steps at a time, the action that swings — is the
// helper's original behaviour, unchanged.
#pragma once

#include <cstdint>
#include <string_view>

namespace GameLogic::Automation
{
enum class AttackResult : std::uint8_t
{
    // The swing was issued this call.
    Attacked,
    // Out of range: a step towards the target was taken instead.
    Approaching,
    // A swing is still playing; try again on a later frame.
    Busy,
    // No target was given.
    NoTarget,
    // The client does not know an object with that id.
    NotInView,
    // Dead, or a player while players are not allowed.
    NotAttackable,
    // The path finder found no way into range.
    NoPath,
    // In range, but a wall stands between the two.
    Blocked,
    // A bow or crossbow is equipped and the quiver is empty.
    NoArrows,
    // The character stands in a safe zone, where the game refuses attacks.
    SafeZone,
};

// Plain-attack reach in tiles. Named once here; the helper used to carry
// its own copies.
inline constexpr float BasicAttackRangeDefault = 1.8f;
inline constexpr float BasicAttackRangeSpear = 2.2f;
inline constexpr float BasicAttackRangeBow = 6.0f;

// The plain-attack reach of what the hero holds now, in tiles.
float EquippedAttackRange();

// A bow outranges a spear, which outreaches everything else.
[[nodiscard]] constexpr float BasicAttackRange(bool spearEquipped, bool bowEquipped)
{
    if (bowEquipped)
    {
        return BasicAttackRangeBow;
    }
    return spearEquipped ? BasicAttackRangeSpear : BasicAttackRangeDefault;
}

[[nodiscard]] constexpr std::string_view ResultName(AttackResult result)
{
    switch (result)
    {
    case AttackResult::Attacked:
        return "attacked";
    case AttackResult::Approaching:
        return "approaching";
    case AttackResult::Busy:
        return "busy";
    case AttackResult::NoTarget:
        return "no_target";
    case AttackResult::NotInView:
        return "not_in_view";
    case AttackResult::NotAttackable:
        return "not_attackable";
    case AttackResult::NoPath:
        return "no_path";
    case AttackResult::Blocked:
        return "blocked";
    case AttackResult::NoArrows:
        return "no_arrows";
    case AttackResult::SafeZone:
        return "safe_zone";
    }
    return "unknown";
}

// Whether a caller that keeps a target list should drop this target: the
// outcomes the auto-helper has always treated as "forget it and pick
// another one".
[[nodiscard]] constexpr bool ShouldForgetTarget(AttackResult result)
{
    return result == AttackResult::NotInView || result == AttackResult::NotAttackable ||
           result == AttackResult::NoPath || result == AttackResult::Blocked;
}

// The walk into range of a target takes one step per call, to the free tile next to the character that lies
// nearest to the target, with no look further ahead. With a wall or a fence between the two the nearest tile is the
// one at the wall, then the one beside it, then the first again: the character stepped to and fro along the fence
// without ever reaching the target or giving it up, while monsters on its own side waited (Tyres in the Arena pen,
// 2026-11-10). ApproachWatch tells such a walk from one that gets there: the gap to the target (any measure that
// falls as the character gets closer) has to reach a new low within `patience`.
struct ApproachWatch
{
    int target = -1;
    int bestGap = 0;
    long long closerAt = 0; // when the gap last reached a new low
    long long seenAt = 0;   // the last step towards this target

    // Notes a step towards `targetKey` at `now` (any time unit) and returns whether the approach should be given
    // up. A step after a pause longer than `pause` starts a new approach: the character did something else meanwhile.
    [[nodiscard]] constexpr bool NoteStep(int targetKey, int gap, long long now, long long patience, long long pause)
    {
        const bool fresh = targetKey != target || now - seenAt > pause;
        seenAt = now;
        if (fresh || gap < bestGap)
        {
            target = targetKey;
            bestGap = gap;
            closerAt = now;
            return false;
        }
        if (now - closerAt < patience)
        {
            return false;
        }
        target = -1;
        return true;
    }

    constexpr void Reset()
    {
        target = -1;
    }
};

// Which of two targets a caller that picks the nearest should prefer: one with a clear line to it comes before one
// behind a wall, however near that one is; among equals the nearer one, and at the same distance the later one, as
// the helper's choice always did.
[[nodiscard]] constexpr bool IsBetterTarget(bool clearLine, int distance, bool bestClearLine, int bestDistance)
{
    if (clearLine != bestClearLine)
    {
        return clearLine;
    }
    return distance <= bestDistance;
}

// True while a swing animation is still playing, so callers keep the
// cadence of AttackSpeed instead of their own timer.
[[nodiscard]] bool IsSwingInProgress();

// Called right after an automated attack or skill was sent. Until a swing shows (or a short wait runs out)
// IsSwingInProgress stays true, so a send that starts no animation is not repeated on every frame.
void NoteAutomatedSend();

// Forgets the last automated send, so IsSwingInProgress no longer waits for its swing to show.
void ForgetAutomatedSend();

// Attacks the object with that server-assigned id, walking into range
// first when needed. `huntingDistance` bounds how far the path finder may
// search beyond the weapon's reach.
AttackResult AttackObject(int targetKey, bool allowPlayers, int huntingDistance);
} // namespace GameLogic::Automation
