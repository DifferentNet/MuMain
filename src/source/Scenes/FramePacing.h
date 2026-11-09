#pragma once

#include <algorithm>

// How the main loop waits for the next frame under a frame rate cap (Scenes/SceneCore.cpp,
// WaitForNextActivity). A sleep costs no CPU but may overrun; a yield loop is exact but keeps a core
// busy. The loop asks again after every sleep, so one answer covers at most kMaxSleepMs: packets, the
// window's messages and the control socket are served in between.
namespace Scenes::FramePacing
{
// Longest single sleep, so the loop keeps serving the network while a slow cap waits.
inline constexpr double kMaxSleepMs = 10.0;

enum class Precision
{
    Coarse,  // the system timer has its default period (about 15.6 ms on Windows): a sleep may overrun by that
    Fine,    // the timer period is 1 ms: a sleep overruns by a millisecond or two at most
    Relaxed, // nobody sees the frames (minimized window): a frame may start a few milliseconds late
};

// A Relaxed wait rounds its last sleep up (up to 1 ms late) and the sleep itself overruns by a
// millisecond or two. Whoever needs `fps` as the lowest rate (the game's logic slows below its
// reference rate) caps a Relaxed loop this much earlier, so a late frame is still in time.
inline constexpr double kRelaxedSlackMs = 4.0;

constexpr double RelaxedTargetFps(double fps)
{
    const double periodMs = 1000.0 / fps;
    return periodMs > 2.0 * kRelaxedSlackMs ? 1000.0 / (periodMs - kRelaxedSlackMs) : fps;
}

// Whole milliseconds to sleep now, `restMs` before the next frame is due; 0 = yield instead.
//  - Coarse: sleeps while 20 ms or more are left, 4 ms short of the frame.
//  - Fine: sleeps while 3 ms or more are left, 2 ms short of the frame, so only the last 2-3 ms are
//    a yield loop. (It was 5 ms: a third of a core at a 60 FPS cap.)
//  - Relaxed: sleeps the whole rest and never spins.
constexpr long SleepMsBeforeNextFrame(double restMs, Precision precision)
{
    if (!(restMs > 0.0))
    {
        return 0;
    }
    if (precision == Precision::Relaxed)
    {
        // Rounded up: a wake-up just before the frame is due would only yield until it is.
        const double capped = std::min(restMs, kMaxSleepMs);
        const long whole = static_cast<long>(capped);
        return static_cast<double>(whole) < capped ? whole + 1 : whole;
    }
    const double marginMs = precision == Precision::Fine ? 2.0 : 4.0;
    const double shortestMs = precision == Precision::Fine ? 1.0 : 16.0;
    const double sleepMs = restMs - marginMs;
    if (sleepMs < shortestMs)
    {
        return 0;
    }
    return static_cast<long>(std::min(sleepMs, kMaxSleepMs));
}
} // namespace Scenes::FramePacing
