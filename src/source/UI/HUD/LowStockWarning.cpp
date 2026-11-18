#include "UI/HUD/LowStockWarning.h"

namespace UI::Hud
{
bool LowStockWarning::Track(int itemType, int itemLevel, int count)
{
    if (!m_known || itemType != m_itemType || itemLevel != m_itemLevel)
    {
        m_known = true;
        m_itemType = itemType;
        m_itemLevel = itemLevel;
        m_armed = count > Threshold;
        return false;
    }

    if (count > Threshold)
    {
        m_armed = true;
        return false;
    }

    const bool fell = m_armed && count > 0;
    m_armed = false;
    return fell;
}

void LowStockWarning::Follow(int itemType, int itemLevel, int count)
{
    (void)Track(itemType, itemLevel, count);
}

bool LowStockWarning::Note(int itemType, int itemLevel, int count, std::uint32_t nowMs)
{
    if (!Track(itemType, itemLevel, count))
        return false;
    if (m_said && nowMs - m_saidAt < MinGapMs)
        return false;
    m_said = true;
    m_saidAt = nowMs;
    return true;
}
} // namespace UI::Hud
