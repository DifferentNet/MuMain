#include <doctest.h>

#include "Core/Input/GamepadPageLatch.h"

namespace Pad = Core::Input::Gamepad;

TEST_CASE("a button pressed with the trigger up keeps the first page while held [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK_FALSE(latch.Alt(true, 1, false));
    // The trigger pulled while X is still held: X stays skill 1.
    CHECK_FALSE(latch.Alt(true, 1, true));
    CHECK_FALSE(latch.Alt(true, 1, true));
}

TEST_CASE("a button pressed with the trigger held keeps the second page after the trigger goes [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Alt(true, 1, true));
    // RT let go before X: X stays skill 4, no press of skill 1.
    CHECK(latch.Alt(true, 1, false));
    CHECK(latch.Alt(true, 1, false));
}

TEST_CASE("a press after the button was let go takes the page up then [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Alt(true, 1, true));
    CHECK(latch.Alt(false, 1, false) == true);
    CHECK_FALSE(latch.Alt(true, 2, false));
    CHECK(latch.Alt(false, 2, true) == false);
    CHECK(latch.Alt(true, 3, true));
}

TEST_CASE("a release and a press between two frames count as a new press [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK_FALSE(latch.Alt(true, 1, false));
    // Seen held both frames, yet the counter moved: it was let go and pressed again with RT held.
    CHECK(latch.Alt(true, 2, true));
}

TEST_CASE("a tap shorter than a frame acts on the page up now [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Alt(false, 1, true));
    CHECK_FALSE(latch.Alt(false, 2, false));
}
