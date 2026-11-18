#include "Core/Input/GamepadPageLatch.h"

namespace Core::Input::Gamepad
{
int PageLatch::Page(bool held, std::uint32_t presses, int pageNow)
{
    // A new press (seen now, or let go and pressed again between two frames) takes the page up now;
    // a hold keeps the page it began on, whatever the triggers and bumpers do meanwhile.
    const bool fresh = !m_Latched || presses != m_Presses;
    if (fresh)
    {
        m_Page = pageNow;
        m_Presses = presses;
    }
    m_Latched = held;
    return m_Page;
}
} // namespace Core::Input::Gamepad
