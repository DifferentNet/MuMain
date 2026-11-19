// WarpListMarks.cpp - which rows of the warp list (M) are marked "you are here" and "you died here".

#include "UI/HUD/WarpListMarks.h"

#include <limits>

namespace UI::MoveCommand::Marks
{
int NearestRow(std::span<const RowSpot> rows, int world, int x, int y)
{
    if (world < 0)
        return -1;

    int best = -1;
    long long bestDistance = std::numeric_limits<long long>::max();
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const RowSpot& row = rows[i];
        if (row.world != world)
            continue;
        const long long dx = row.x - x;
        const long long dy = row.y - y;
        const long long distance = dx * dx + dy * dy;
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void DeathSpot::NoteDeath(int world, int x, int y, std::uint32_t now)
{
    m_known = world >= 0;
    m_revived = false;
    m_world = world;
    m_x = x;
    m_y = y;
    m_diedAt = now;
}

void DeathSpot::NoteRevival()
{
    if (!m_known)
        return;
    if (m_revived)
    {
        m_known = false;
        return;
    }
    m_revived = true;
}

void DeathSpot::NoteWarp()
{
    if (m_known && m_revived)
        m_known = false;
}

bool DeathSpot::Get(std::uint32_t now, int& world, int& x, int& y) const
{
    if (!m_known || now - m_diedAt >= KeepMs)
        return false;
    world = m_world;
    x = m_x;
    y = m_y;
    return true;
}

DeathSpot& HeroDeathSpot()
{
    static DeathSpot spot;
    return spot;
}
} // namespace UI::MoveCommand::Marks
