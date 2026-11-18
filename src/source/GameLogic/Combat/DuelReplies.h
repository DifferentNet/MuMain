#pragma once

#include <cstdint>

// What the server's duel answers mean to the player (OpenMU DuelStartResultType and DuelEnded).
namespace GameLogic::Combat
{
enum class DuelStartReply
{
    Accepted,       // 0
    Declined,       // 15
    ArenaFull,      // 16
    TooLowLevel,    // 12 (28 in older servers)
    NotEnoughZen,   // 30
    Failed,         // 14: the duel could not start (the server sends "duel ended" just before it)
    Unknown,
};

[[nodiscard]] DuelStartReply ClassifyDuelStart(int result);

// The server sends "duel ended" to both duellists and then at once to everybody in the arena: after a finished
// duel too, and before a failed start. "The duel has been canceled" is said once per such burst, only for a duel
// that ended without a winner: one that began (start answer 0), or one broken while the hero stood in the arena
// (the countdown before the start answer).
class DuelEndNotice
{
public:
    // "Duel ended" packets closer together than this are one burst.
    static constexpr std::uint32_t BurstMs = 2'000;

    // The duel began (start answer 0).
    void Started();
    // The winner was announced.
    void Finished();
    // A new game (character switch, relog).
    void Reset();
    // A "duel ended" packet: whether to say the duel was canceled.
    [[nodiscard]] bool OnEnded(bool heroInArena, std::uint32_t nowMs);

private:
    bool m_on = false;
    bool m_finished = false;
    bool m_hadEnd = false;
    std::uint32_t m_lastEndMs = 0;
};
} // namespace GameLogic::Combat
