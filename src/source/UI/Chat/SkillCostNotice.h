#pragma once

// SkillCostNotice.h - "not enough mana / AG" when a skill cannot be paid.

#include <cstdint>

namespace UI::Chat::SkillCost
{
// At most one line per SayEveryMs, shared by the touch buttons and the mouse
// and keyboard, so a held button does not bury the chat.
class SayGate
{
public:
    static constexpr std::uint32_t SayEveryMs = 6'000;

    // Whether a line may be said at `nowMs` (GetTickCount); a yes starts the wait.
    [[nodiscard]] bool Allow(std::uint32_t nowMs)
    {
        if (m_said && nowMs - m_saidAt < SayEveryMs)
        {
            return false;
        }

        m_said = true;
        m_saidAt = nowMs;
        return true;
    }

private:
    bool m_said = false;
    std::uint32_t m_saidAt = 0;
};

// Why CanExecuteSkill may refuse a cast before it looks at the mana. When any of these holds, the
// refusal is not about the cost, so "not enough mana / AG" would be wrong even if the mana is low.
struct RefusalContext
{
    bool dead = false;            // the hero is dead
    bool safeZone = false;        // no skills in a safe zone
    bool cursedTemple = false;    // Cursed Temple has its own target rules
    bool useConditionMet = true;  // CheckSkillUseCondition: Blood Castle skills, Brand buff
};

[[nodiscard]] inline bool RefusalMayBeAboutCost(const RefusalContext& context)
{
    return !context.dead && !context.safeZone && !context.cursedTemple && context.useConditionMet;
}

// Says that the mana (or else the AG) is short, unless one was said lately.
void SayShort(bool manaShort);

// A mouse or keyboard cast that ExecuteSkill refused: says so when the skill
// lacks mana with no mana potion to drink, or lacks AG. Silent while MU Helper
// plays, and when the cast was refused before the cost was checked (RefusalContext).
void NoteRefusedCast(int skill);
} // namespace UI::Chat::SkillCost
