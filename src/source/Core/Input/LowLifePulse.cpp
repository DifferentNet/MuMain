#include "Core/Input/LowLifePulse.h"

namespace Core::Input::Gamepad
{
LowLifePulse::Pulse LowLifePulse::Note(float lifeShare, bool dead, Clock::time_point now, Clock::time_point busyUntil)
{
    const float before = m_LastShare;
    const bool known = m_Known;
    m_Known = true;
    m_LastShare = lifeShare;
    if (dead)
    {
        m_FirstDue = false;
        m_SecondDue = false;
        return Pulse::None;
    }
    if (m_FirstDue && now >= m_FirstAt)
    {
        m_FirstDue = false;
        m_SecondDue = true;
        m_SecondAt = now + SecondDelay;
        return Pulse::First;
    }
    if (m_SecondDue && now >= m_SecondAt)
    {
        m_SecondDue = false;
        return Pulse::Second;
    }
    if (!known)
        return Pulse::None;
    if (lifeShare >= ReArmShare)
        m_Armed = true;
    const bool fell = before > ArmShare && lifeShare <= ArmShare;
    const bool rested = !m_Paired || now - m_LastPair >= PairGap;
    if (!fell || !m_Armed || !rested)
        return Pulse::None;
    m_Armed = false;
    m_Paired = true;
    m_LastPair = now;
    if (busyUntil > now)
    {
        // A blow shakes the pad now: the first beat after it.
        m_FirstDue = true;
        m_FirstAt = busyUntil;
        return Pulse::None;
    }
    m_SecondDue = true;
    m_SecondAt = now + SecondDelay;
    return Pulse::First;
}

void LowLifePulse::Forget()
{
    m_Known = false;
    m_FirstDue = false;
    m_SecondDue = false;
}
} // namespace Core::Input::Gamepad
