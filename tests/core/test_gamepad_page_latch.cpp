#include <doctest.h>

#include "Core/Input/GamepadPageLatch.h"

namespace Pad = Core::Input::Gamepad;

TEST_CASE("a button pressed with the trigger up keeps the first page while held [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Page(true, 1, 0) == 0);
    // The trigger pulled while X is still held: X stays skill 1.
    CHECK(latch.Page(true, 1, 1) == 0);
    CHECK(latch.Page(true, 1, 1) == 0);
}

TEST_CASE("a button pressed with the trigger held keeps the second page after the trigger goes [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Page(true, 1, 1) == 1);
    // RT let go before X: X stays skill 4, no press of skill 1.
    CHECK(latch.Page(true, 1, 0) == 1);
    CHECK(latch.Page(true, 1, 0) == 1);
}

TEST_CASE("a press after the button was let go takes the page up then [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Page(true, 1, 1) == 1);
    CHECK(latch.Page(false, 1, 0) == 1);
    CHECK(latch.Page(true, 2, 0) == 0);
    CHECK(latch.Page(false, 2, 1) == 0);
    CHECK(latch.Page(true, 3, 1) == 1);
}

TEST_CASE("a release and a press between two frames count as a new press [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Page(true, 1, 0) == 0);
    // Seen held both frames, yet the counter moved: it was let go and pressed again with RT held.
    CHECK(latch.Page(true, 2, 1) == 1);
}

TEST_CASE("a tap shorter than a frame acts on the page up now [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Page(false, 1, 1) == 1);
    CHECK(latch.Page(false, 2, 0) == 0);
}

TEST_CASE("a button pressed with the bumper held keeps the third page after the bumper goes [core][gamepad]")
{
    Pad::PageLatch latch;
    CHECK(latch.Page(true, 1, 2) == 2);
    // RB let go before X (RT still held): X stays skill 7, no press of skill 4.
    CHECK(latch.Page(true, 1, 1) == 2);
    CHECK(latch.Page(false, 1, 1) == 2);
    CHECK(latch.Page(true, 2, 1) == 1);
}
