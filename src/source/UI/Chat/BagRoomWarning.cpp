#include "UI/Chat/BagRoomWarning.h"

namespace UI::Chat
{
BagRoomWarning::Say BagRoomWarning::Note(int freeCells)
{
    if (freeCells > AlmostFullCells)
    {
        m_saidAlmostFull = false;
        m_saidFull = false;
        return Say::Nothing;
    }

    if (freeCells <= 0)
    {
        if (m_saidFull)
            return Say::Nothing;
        m_saidFull = true;
        m_saidAlmostFull = true;
        return Say::Full;
    }

    if (m_saidAlmostFull)
        return Say::Nothing;
    m_saidAlmostFull = true;
    return Say::AlmostFull;
}
} // namespace UI::Chat
