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

// Says that the mana (or else the AG) is short, unless one was said lately.
void SayShort(bool manaShort);

// A mouse or keyboard cast that ExecuteSkill refused: says so when the skill
// lacks mana with no mana potion to drink, or lacks AG. Silent while MU Helper
// plays.
void NoteRefusedCast(int skill);
} // namespace UI::Chat::SkillCost
