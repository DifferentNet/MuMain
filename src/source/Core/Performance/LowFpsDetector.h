#pragma once

#include <array>
#include <cstdint>

// Watches the frame times of steady play and tells when the frame rate has been really low for a while: the median
// frame of two 20-second windows in a row under the bar (30 FPS, or 70 % of a known cap when that is lower). The
// median, not the average, so a few long frames (a teleport, a big fight's spike) don't make a window slow, and a
// few short ones don't make a slow window good. Frames longer than half a second are dropped: alt-tab, a hitch while
// a map loads or a window opens is not the scene's frame rate. Pure: the caller says what steady means and when the
// clock restarts. See docs/enhanced-look.md, "Low frame rate hint".
namespace Core::Performance
{

struct LowFpsRules
{
    // How long one window measures, in milliseconds of steady play.
    double windowMs = 20000.0;
    // Slow windows in a row before the hint is due.
    int slowWindowsInRow = 2;
    // After entering the world nothing is measured for this long: the first textures, models and shaders load then.
    double warmupMs = 45000.0;
    // The same after a map change, a preset change or the window getting the focus back.
    double quietMs = 20000.0;
    // A frame longer than this is dropped (a stall, not the scene's frame rate).
    float stallFrameMs = 500.0f;
    // A window with fewer frames than this counts neither way (mostly stalls).
    int minFramesPerWindow = 40;
    // The bar: under this many frames per second...
    float slowFps = 30.0f;
    // ...or under this share of a known cap when that is lower (a 30 FPS cap is not slow at 28).
    float capShare = 0.7f;
};

// What one frame's measurement says.
struct LowFpsFrameResult
{
    // A window just ended with enough frames; its median is in windowMedianFps.
    bool windowCompleted = false;
    float windowMedianFps = 0.0f;
    // That window was under the bar.
    bool windowSlow = false;
    // Enough slow windows in a row: the hint is due (the count starts again).
    bool hintDue = false;
};

class LowFpsDetector
{
public:
    explicit LowFpsDetector(LowFpsRules rules = {});

    // Once per rendered frame: the clock in milliseconds, the frame's own time, whether the game is in steady play
    // (anything else clears the window and the slow count) and the FPS cap (0 or less when there is none or it is
    // unknown).
    LowFpsFrameResult OnFrame(double nowMs, float frameMs, bool steady, double capFps);

    // The world was entered: nothing is measured for rules.warmupMs from now.
    void StartWarmup(double nowMs);
    // A map, preset or focus change: nothing is measured for rules.quietMs from now (a longer warm-up stays).
    void StartQuiet(double nowMs);
    // Forgets the window, the slow count and any warm-up or quiet time.
    void Reset();

    void SetRules(const LowFpsRules& rules) { m_rules = rules; }
    const LowFpsRules& Rules() const { return m_rules; }

    int SlowWindowsInRow() const { return m_slowWindows; }
    int FramesInWindow() const { return m_framesInWindow; }
    // The clock time measuring starts again at (0 when it isn't waiting).
    double MeasuringFromMs() const { return m_measureFromMs; }

    // The FPS a window's median must reach not to be slow, for a cap (0 or less: none or unknown).
    static float SlowBarFps(const LowFpsRules& rules, double capFps);

    // Four buckets per millisecond (a quarter of a millisecond each) up to 512 ms, past the stall limit.
    static constexpr int BucketsPerMs = 4;
    static constexpr int HistogramBuckets = 512 * BucketsPerMs;

private:
    void ClearWindow();
    void ClearAll();
    float MedianFrameMs() const;
    LowFpsFrameResult FinishWindow(double capFps);

    LowFpsRules m_rules;
    std::array<std::uint32_t, HistogramBuckets> m_histogram{};
    int m_framesInWindow = 0;
    double m_windowStartMs = 0.0;
    bool m_windowOpen = false;
    int m_slowWindows = 0;
    double m_measureFromMs = 0.0;
};

} // namespace Core::Performance
