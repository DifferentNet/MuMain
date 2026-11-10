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

// VSync that paces nothing. With VSync on the game sets no cap of its own (SetTargetFps): the display
// paces the frames. A display that is asleep or switched off, a remote session or a driver setting
// can let every present return at once, and the loop then draws as many frames as a CPU core gives
// (measured 2026-11-10: about 800 FPS, a whole core, VSync on). The watch counts the frames of each
// half second; a rate far above the display's means VSync is not pacing, and it asks for a cap at the
// display's rate. Every 15 s it lifts the cap for one half second to see whether VSync paces again.
// It counts every pass of the loop that reaches the renderer, also one whose frame the renderer then
// skips because the swapchain gave no image: those passes are the wasted core as well.
class VsyncWatch
{
public:
    static constexpr double kWindowMs = 500.0;
    static constexpr double kProbeEveryMs = 15000.0;
    // VSync never shows more frames than the display has; half as many again is past any jitter.
    static constexpr double kOverRefresh = 1.5;
    // No display rate known: above every display sold, and a cap no player on a fast display minds.
    static constexpr double kUnknownLimitFps = 700.0;
    static constexpr double kUnknownCapFps = 144.0;
    // A display that reports a wrongly low rate must not slow the game down.
    static constexpr double kMinCapFps = 60.0;

    // Call once per drawn frame. `refreshHz`: the fastest connected display, 0 when unknown.
    // Returns the cap for the next frames in FPS, 0 for none.
    double OnFrame(double nowMs, bool vsyncOn, double refreshHz)
    {
        m_engagedNow = false;
        if (!vsyncOn)
        {
            *this = VsyncWatch{};
            return 0.0;
        }
        if (!m_started || nowMs < m_windowStartMs)
        {
            m_started = true;
            StartWindow(nowMs);
            return m_capFps;
        }
        if (m_capFps > 0.0)
        {
            if (nowMs - m_engagedAtMs < kProbeEveryMs)
            {
                return m_capFps;
            }
            // Probe: one window without the cap.
            m_probing = true;
            m_capFps = 0.0;
            StartWindow(nowMs);
            return 0.0;
        }

        ++m_frames;
        const double elapsedMs = nowMs - m_windowStartMs;
        if (elapsedMs < kWindowMs)
        {
            return 0.0;
        }
        const bool known = refreshHz > 0.0;
        const double rate = static_cast<double>(m_frames) * 1000.0 / elapsedMs;
        if (rate > (known ? refreshHz * kOverRefresh : kUnknownLimitFps))
        {
            m_capFps = !known ? kUnknownCapFps : refreshHz < kMinCapFps ? kMinCapFps : refreshHz;
            m_engagedAtMs = nowMs;
            m_engagedNow = !m_probing;
            m_measuredFps = rate;
        }
        m_probing = false;
        StartWindow(nowMs);
        return m_capFps;
    }

    // True on the frame the cap came on (not when a probe put it back): the moment for a log line.
    bool EngagedNow() const { return m_engagedNow; }
    double MeasuredFps() const { return m_measuredFps; }
    double CapFps() const { return m_capFps; }

private:
    void StartWindow(double nowMs)
    {
        m_windowStartMs = nowMs;
        m_frames = 0;
    }

    bool m_started = false;
    bool m_probing = false;
    bool m_engagedNow = false;
    int m_frames = 0;
    double m_windowStartMs = 0.0;
    double m_engagedAtMs = 0.0;
    double m_capFps = 0.0;
    double m_measuredFps = 0.0;
};
} // namespace Scenes::FramePacing
