#include "Core/Input/GamepadPageLatch.h"

namespace Core::Input::Gamepad
{
bool PageLatch::Alt(bool held, std::uint32_t presses, bool altNow)
{
    // A new press (seen now, or let go and pressed again between two frames) takes the page up now;
    // a hold keeps the page it began on, whatever the trigger does meanwhile.
    const bool fresh = !m_Latched || presses != m_Presses;
    if (fresh)
    {
        m_Alt = altNow;
        m_Presses = presses;
    }
    m_Latched = held;
    return m_Alt;
}
} // namespace Core::Input::Gamepad
