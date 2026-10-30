#include "UI/Mobile/FollowThrough.h"

namespace UI::Mobile
{
void FollowThrough::Press(int control, double seconds)
{
    m_control = control;
    m_since = seconds;
}

int FollowThrough::Owed(double seconds) const
{
    if (m_control == None || seconds - m_since > MaxSeconds)
        return None;
    return m_control;
}

void FollowThrough::Settle(int control)
{
    if (m_control == control)
        m_control = None;
}

void FollowThrough::Drop()
{
    m_control = None;
}
} // namespace UI::Mobile
