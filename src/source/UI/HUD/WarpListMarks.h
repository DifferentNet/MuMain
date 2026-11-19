#pragma once

// WarpListMarks.h - which rows of the warp list (M) are marked "you are here" and "you died here".

#include <cstdint>
#include <span>

namespace UI::MoveCommand::Marks
{
// Where a warp list row lands: its world and the middle of its gate's area. world < 0 when the row's gate is unknown.
struct RowSpot
{
    int world = -1;
    int x = 0;
    int y = 0;
};

// The row on `world` whose landing spot is nearest (x, y), or -1 when no row lands on that world. A map with
// several rows (Lost Tower 1-7, Dungeon 1-3) thus gets the floor the point is on.
[[nodiscard]] int NearestRow(std::span<const RowSpot> rows, int world, int x, int y);

// Where the hero last died outside the events, until he warps anywhere after the respawn or KeepMs pass. The
// server sends only the respawn packet after a death; a second respawn packet without a death (the server moving
// the hero) counts as a warp.
class DeathSpot
{
public:
    static constexpr std::uint32_t KeepMs = 10u * 60u * 1000u;

    void NoteDeath(int world, int x, int y, std::uint32_t now);
    void NoteRevival();
    // The hero changed map or went through a gate.
    void NoteWarp();
    void Clear() { m_known = false; }

    // The spot while it counts; false when there is none.
    [[nodiscard]] bool Get(std::uint32_t now, int& world, int& x, int& y) const;

private:
    bool m_known = false;
    bool m_revived = false;
    int m_world = -1;
    int m_x = 0;
    int m_y = 0;
    std::uint32_t m_diedAt = 0;
};

// The hero's own spot, kept for the whole session.
DeathSpot& HeroDeathSpot();
} // namespace UI::MoveCommand::Marks
