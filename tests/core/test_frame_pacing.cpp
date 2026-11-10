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

TEST_CASE("a relaxed loop is capped early enough to hold the wanted rate [core][frame-pacing]")
{
    using Scenes::FramePacing::RelaxedTargetFps;
    // 25 FPS wanted: frames are due every 36 ms, so one that starts up to 4 ms late keeps 25 FPS.
    CHECK(1000.0 / RelaxedTargetFps(25.0) == doctest::Approx(36.0));
    CHECK(RelaxedTargetFps(25.0) > 25.0);
    // A rate too fast for the slack is left alone.
    CHECK(RelaxedTargetFps(240.0) == doctest::Approx(240.0));
}

namespace
{
using Scenes::FramePacing::VsyncWatch;

// Draws frames at `fps` from `fromMs` for `forMs`, as the game loop would under the watch's cap:
// a frame is drawn no sooner than the cap allows. Returns the cap after the last frame.
double Run(VsyncWatch& watch, double& nowMs, double forMs, double fps, bool vsync, double refreshHz, int* engaged = nullptr)
{
    const double endMs = nowMs + forMs;
    double cap = watch.CapFps();
    while (nowMs < endMs)
    {
        const double freeStep = 1000.0 / fps;
        const double capStep = cap > 0.0 ? 1000.0 / cap : 0.0;
        nowMs += freeStep > capStep ? freeStep : capStep;
        cap = watch.OnFrame(nowMs, vsync, refreshHz);
        if (engaged && watch.EngagedNow())
        {
            ++*engaged;
        }
    }
    return cap;
}
} // namespace

TEST_CASE("VSync that paces the frames is left alone [core][frame-pacing][vsync-watch]")
{
    for (const double hz : {60.0, 75.0, 144.0, 175.0, 240.0})
    {
        VsyncWatch watch;
        double now = 1000.0;
        int engaged = 0;
        CHECK(Run(watch, now, 60000.0, hz, true, hz, &engaged) == 0.0);
        // A slow scene, and a second display slower than the one the window is on.
        CHECK(Run(watch, now, 5000.0, hz / 3.0, true, hz, &engaged) == 0.0);
        CHECK(Run(watch, now, 5000.0, hz * 1.4, true, hz, &engaged) == 0.0);
        CHECK(engaged == 0);
    }
}

TEST_CASE("VSync that paces nothing gets a cap at the display's rate [core][frame-pacing][vsync-watch]")
{
    VsyncWatch watch;
    double now = 0.0;
    int engaged = 0;
    // Measured 2026-11-10: about 800 FPS with VSync on.
    CHECK(Run(watch, now, 1200.0, 800.0, true, 175.0, &engaged) == 175.0);
    CHECK(engaged == 1);
    CHECK(watch.MeasuredFps() == doctest::Approx(800.0).epsilon(0.05));
    // It holds, with a short look every 15 s; the log line is written once.
    for (int i = 0; i < 8; ++i)
    {
        CHECK(Run(watch, now, 14000.0, 800.0, true, 175.0, &engaged) == 175.0);
    }
    CHECK(engaged == 1);
}

TEST_CASE("the cap goes when VSync paces again [core][frame-pacing][vsync-watch]")
{
    VsyncWatch watch;
    double now = 0.0;
    CHECK(Run(watch, now, 2000.0, 900.0, true, 60.0) == 60.0);
    // The display woke up: frames now come at 60 whatever the cap. The next look lifts the cap.
    CHECK(Run(watch, now, 17000.0, 60.0, true, 60.0) == 0.0);
    CHECK(Run(watch, now, 60000.0, 60.0, true, 60.0) == 0.0);
    // And it comes back with a new log line when VSync fails again.
    int engaged = 0;
    CHECK(Run(watch, now, 2000.0, 900.0, true, 60.0, &engaged) == 60.0);
    CHECK(engaged == 1);
}

TEST_CASE("VSync off: the watch does nothing and forgets [core][frame-pacing][vsync-watch]")
{
    VsyncWatch watch;
    double now = 0.0;
    CHECK(Run(watch, now, 5000.0, 2000.0, false, 60.0) == 0.0);
    CHECK(Run(watch, now, 2000.0, 2000.0, true, 60.0) == 60.0);
    CHECK(watch.OnFrame(now + 1.0, false, 60.0) == 0.0);
    CHECK(watch.CapFps() == 0.0);
}

TEST_CASE("an unknown display rate: only a wild frame rate is capped [core][frame-pacing][vsync-watch]")
{
    VsyncWatch watch;
    double now = 0.0;
    CHECK(Run(watch, now, 5000.0, 360.0, true, 0.0) == 0.0);
    CHECK(Run(watch, now, 2000.0, 800.0, true, 0.0) == VsyncWatch::kUnknownCapFps);
}

TEST_CASE("a clock that steps back restarts the watch's window [core][frame-pacing][vsync-watch]")
{
    VsyncWatch watch;
    double now = 100000.0;
    CHECK(Run(watch, now, 300.0, 60.0, true, 60.0) == 0.0);
    now = 10.0;
    CHECK(Run(watch, now, 5000.0, 60.0, true, 60.0) == 0.0);
}
