#include "doctest.h"

#include "Scenes/FramePacing.h"

using Scenes::FramePacing::Precision;
using Scenes::FramePacing::SleepMsBeforeNextFrame;

TEST_CASE("a 1 ms timer sleeps to within 2-3 ms of the frame [core][frame-pacing]")
{
    // 60 FPS cap, the frame took 2 ms: 14.67 ms left.
    CHECK(SleepMsBeforeNextFrame(14.67, Precision::Fine) == 10);
    CHECK(SleepMsBeforeNextFrame(4.67, Precision::Fine) == 2);
    CHECK(SleepMsBeforeNextFrame(3.0, Precision::Fine) == 1);
    // The last stretch is a yield loop, so the frame starts on time.
    CHECK(SleepMsBeforeNextFrame(2.99, Precision::Fine) == 0);
    CHECK(SleepMsBeforeNextFrame(0.5, Precision::Fine) == 0);
}

TEST_CASE("a sleep always leaves its margin before the frame [core][frame-pacing]")
{
    for (double rest = 0.0; rest < 50.0; rest += 0.01)
    {
        const long fine = SleepMsBeforeNextFrame(rest, Precision::Fine);
        CHECK((fine == 0 || rest - static_cast<double>(fine) >= 2.0));
        const long coarse = SleepMsBeforeNextFrame(rest, Precision::Coarse);
        CHECK((coarse == 0 || rest - static_cast<double>(coarse) >= 4.0));
        CHECK(fine <= 10);
        CHECK(coarse <= 10);
        CHECK(SleepMsBeforeNextFrame(rest, Precision::Relaxed) <= 10);
    }
}

TEST_CASE("the default timer sleeps only far from the frame [core][frame-pacing]")
{
    CHECK(SleepMsBeforeNextFrame(38.0, Precision::Coarse) == 10);
    CHECK(SleepMsBeforeNextFrame(20.0, Precision::Coarse) == 10);
    CHECK(SleepMsBeforeNextFrame(19.9, Precision::Coarse) == 0);
}

TEST_CASE("a minimized window never spins [core][frame-pacing]")
{
    // 25 FPS: 40 ms a frame, slept in pieces of at most 10 ms so packets are served in between.
    CHECK(SleepMsBeforeNextFrame(39.0, Precision::Relaxed) == 10);
    CHECK(SleepMsBeforeNextFrame(9.2, Precision::Relaxed) == 10);
    CHECK(SleepMsBeforeNextFrame(3.0, Precision::Relaxed) == 3);
    CHECK(SleepMsBeforeNextFrame(0.2, Precision::Relaxed) == 1);
}

TEST_CASE("no time left means no sleep [core][frame-pacing]")
{
    for (const Precision precision : {Precision::Coarse, Precision::Fine, Precision::Relaxed})
    {
        CHECK(SleepMsBeforeNextFrame(0.0, precision) == 0);
        CHECK(SleepMsBeforeNextFrame(-3.0, precision) == 0);
    }
}
