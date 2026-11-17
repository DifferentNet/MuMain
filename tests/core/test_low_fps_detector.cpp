#include <doctest.h>

#include "Core/Performance/LowFpsDetector.h"
#include "Render/Look/LowFpsHintPolicy.h"

using Core::Performance::LowFpsDetector;
using Core::Performance::LowFpsFrameResult;
using Core::Performance::LowFpsRules;
namespace Hint = Render::Look::LowFpsHint;

namespace
{
constexpr double NoCap = 0.0;

// What feeding frames reported: every window that ended and whether the hint came due.
struct Fed
{
    int windows = 0;
    int slowWindows = 0;
    int hints = 0;
    float lastMedianFps = 0.0f;
};

// Frames of `frameMs` each for `durationMs`, from `clockMs` on (advanced).
Fed Feed(LowFpsDetector& detector, double& clockMs, float frameMs, double durationMs, bool steady = true,
         double capFps = NoCap)
{
    Fed fed;
    const double end = clockMs + durationMs;
    while (clockMs < end)
    {
        clockMs += frameMs;
        const LowFpsFrameResult result = detector.OnFrame(clockMs, frameMs, steady, capFps);
        if (result.windowCompleted)
        {
            ++fed.windows;
            fed.lastMedianFps = result.windowMedianFps;
            if (result.windowSlow)
                ++fed.slowWindows;
        }
        if (result.hintDue)
            ++fed.hints;
    }
    return fed;
}

float FpsMs(float fps)
{
    return 1000.0f / fps;
}

constexpr double Window = 20000.0;
} // namespace

TEST_CASE("two slow windows in a row make the hint due [core][lowfps]")
{
    LowFpsDetector detector;
    double clock = 0.0;
    const Fed first = Feed(detector, clock, FpsMs(20.0f), Window + 100.0);
    CHECK(first.windows == 1);
    CHECK(first.slowWindows == 1);
    CHECK(first.hints == 0);
    CHECK(detector.SlowWindowsInRow() == 1);

    const Fed second = Feed(detector, clock, FpsMs(20.0f), Window);
    CHECK(second.windows == 1);
    CHECK(second.hints == 1);
    CHECK(detector.SlowWindowsInRow() == 0);
    CHECK(second.lastMedianFps == doctest::Approx(20.0f).epsilon(0.03));
}

TEST_CASE("a good window between slow ones resets the row [core][lowfps]")
{
    LowFpsDetector detector;
    double clock = 0.0;
    Feed(detector, clock, FpsMs(20.0f), Window + 100.0);
    const Fed good = Feed(detector, clock, FpsMs(60.0f), Window);
    CHECK(good.windows == 1);
    CHECK(good.slowWindows == 0);
    CHECK(detector.SlowWindowsInRow() == 0);
    const Fed slowAgain = Feed(detector, clock, FpsMs(20.0f), Window + 100.0);
    CHECK(slowAgain.hints == 0);
    CHECK(detector.SlowWindowsInRow() == 1);
}

TEST_CASE("nothing is measured during the warm-up and the quiet time [core][lowfps]")
{
    LowFpsDetector detector;
    double clock = 1000.0;
    detector.StartWarmup(clock);
    const Fed warmup = Feed(detector, clock, FpsMs(10.0f), 44000.0);
    CHECK(warmup.windows == 0);
    CHECK(detector.FramesInWindow() == 0);

    // Past the warm-up two slow windows still take 40 s.
    const Fed after = Feed(detector, clock, FpsMs(10.0f), 2 * Window + 2000.0);
    CHECK(after.hints == 1);

    detector.StartQuiet(clock);
    const Fed quiet = Feed(detector, clock, FpsMs(10.0f), 19000.0);
    CHECK(quiet.windows == 0);
    CHECK(detector.SlowWindowsInRow() == 0);
}

TEST_CASE("a quiet time never shortens a longer warm-up [core][lowfps]")
{
    LowFpsDetector detector;
    detector.StartWarmup(0.0);
    detector.StartQuiet(1000.0);
    CHECK(detector.MeasuringFromMs() == doctest::Approx(45000.0));
}

TEST_CASE("leaving steady play clears the window and the row [core][lowfps]")
{
    LowFpsDetector detector;
    double clock = 0.0;
    Feed(detector, clock, FpsMs(20.0f), Window + 100.0);
    CHECK(detector.SlowWindowsInRow() == 1);
    Feed(detector, clock, FpsMs(20.0f), 5000.0);
    CHECK(detector.FramesInWindow() > 0);

    Feed(detector, clock, FpsMs(20.0f), 100.0, false);
    CHECK(detector.SlowWindowsInRow() == 0);
    CHECK(detector.FramesInWindow() == 0);

    const Fed again = Feed(detector, clock, FpsMs(20.0f), Window + 100.0);
    CHECK(again.hints == 0);
}

TEST_CASE("stalls are dropped and a window of mostly stalls counts neither way [core][lowfps]")
{
    LowFpsDetector detector;
    double clock = 0.0;
    // 60 FPS with a 2-second hitch in the middle: still a good window.
    Feed(detector, clock, FpsMs(60.0f), 8000.0);
    clock += 2000.0;
    detector.OnFrame(clock, 2000.0f, true, NoCap);
    const Fed rest = Feed(detector, clock, FpsMs(60.0f), 11000.0);
    CHECK(rest.windows == 1);
    CHECK(rest.slowWindows == 0);

    // Only stalls: no frames counted, the window is thrown away and breaks the row.
    Feed(detector, clock, FpsMs(20.0f), Window + 100.0);
    CHECK(detector.SlowWindowsInRow() == 1);
    const Fed stalls = Feed(detector, clock, 900.0f, Window + 1000.0);
    CHECK(stalls.windows == 0);
    CHECK(detector.SlowWindowsInRow() == 0);
}

TEST_CASE("a window with too few frames counts neither way [core][lowfps]")
{
    LowFpsRules rules;
    rules.minFramesPerWindow = 100;
    LowFpsDetector detector(rules);
    double clock = 0.0;
    // 4 FPS: 80 frames in 20 s, under the 100 the rules ask for.
    const Fed fed = Feed(detector, clock, FpsMs(4.0f), Window + 300.0);
    CHECK(fed.windows == 0);
}

TEST_CASE("the bar follows a known cap only when 70 % of it is under 30 FPS [core][lowfps]")
{
    const LowFpsRules rules;
    CHECK(LowFpsDetector::SlowBarFps(rules, 24.0) == doctest::Approx(16.8f));
    CHECK(LowFpsDetector::SlowBarFps(rules, 30.0) == doctest::Approx(21.0f));
    CHECK(LowFpsDetector::SlowBarFps(rules, 60.0) == doctest::Approx(30.0f));
    CHECK(LowFpsDetector::SlowBarFps(rules, NoCap) == doctest::Approx(30.0f));
    CHECK(LowFpsDetector::SlowBarFps(rules, -1.0) == doctest::Approx(30.0f));
    // VSync caps at the display's rate.
    CHECK(LowFpsDetector::SlowBarFps(rules, 60.0) == doctest::Approx(30.0f));
    CHECK(LowFpsDetector::SlowBarFps(rules, 144.0) == doctest::Approx(30.0f));
}

TEST_CASE("a 30 FPS cap played at 28 is not slow, uncapped 28 is [core][lowfps]")
{
    LowFpsDetector capped;
    double clock = 0.0;
    const Fed atCap = Feed(capped, clock, FpsMs(28.0f), Window + 100.0, true, 30.0);
    CHECK(atCap.windows == 1);
    CHECK(atCap.slowWindows == 0);

    LowFpsDetector uncapped;
    clock = 0.0;
    const Fed uncappedFed = Feed(uncapped, clock, FpsMs(28.0f), Window + 100.0);
    CHECK(uncappedFed.slowWindows == 1);

    // A 24 FPS cap: 20 is fine, 15 is slow.
    LowFpsDetector cap24;
    clock = 0.0;
    CHECK(Feed(cap24, clock, FpsMs(20.0f), Window + 100.0, true, 24.0).slowWindows == 0);
    CHECK(Feed(cap24, clock, FpsMs(15.0f), Window, true, 24.0).slowWindows == 1);
}

TEST_CASE("steady 30 FPS without a cap is not slow, 29 FPS is [core][lowfps]")
{
    LowFpsDetector at30;
    double clock = 0.0;
    const Fed thirty = Feed(at30, clock, FpsMs(30.0f), Window + 100.0);
    CHECK(thirty.windows == 1);
    CHECK(thirty.slowWindows == 0);
    CHECK(thirty.lastMedianFps >= 30.0f);

    LowFpsDetector at29;
    clock = 0.0;
    const Fed twentyNine = Feed(at29, clock, FpsMs(29.0f), Window + 100.0);
    CHECK(twentyNine.windows == 1);
    CHECK(twentyNine.slowWindows == 1);
    CHECK(twentyNine.lastMedianFps < 30.0f);
    CHECK(twentyNine.lastMedianFps == doctest::Approx(29.0f).epsilon(0.01));
}

TEST_CASE("the median ignores spikes the average would not [core][lowfps]")
{
    LowFpsDetector detector;
    double clock = 0.0;
    // 60 FPS with every fourth frame at 300 ms: the average is ~10 FPS, the median 60.
    Fed fed;
    int frame = 0;
    while (clock < Window + 100.0)
    {
        const float ms = (++frame % 4 == 0) ? 300.0f : FpsMs(60.0f);
        clock += ms;
        const LowFpsFrameResult result = detector.OnFrame(clock, ms, true, NoCap);
        if (result.windowCompleted)
        {
            ++fed.windows;
            fed.lastMedianFps = result.windowMedianFps;
            fed.slowWindows += result.windowSlow ? 1 : 0;
        }
    }
    CHECK(fed.windows == 1);
    CHECK(fed.slowWindows == 0);
    CHECK(fed.lastMedianFps > 55.0f);
}

TEST_CASE("Reset forgets the row and the warm-up [core][lowfps]")
{
    LowFpsDetector detector;
    double clock = 0.0;
    Feed(detector, clock, FpsMs(20.0f), Window + 100.0);
    detector.StartWarmup(clock);
    detector.Reset();
    CHECK(detector.SlowWindowsInRow() == 0);
    CHECK(detector.MeasuringFromMs() == 0.0);
}

TEST_CASE("the hint steps down one preset at a time and never below Low [render][lowfps]")
{
    CHECK(Hint::LowerStep(GraphicsPreset::Ultra) == GraphicsPreset::High);
    CHECK(Hint::LowerStep(GraphicsPreset::High) == GraphicsPreset::Medium);
    CHECK(Hint::LowerStep(GraphicsPreset::Medium) == GraphicsPreset::Low);
    CHECK_FALSE(Hint::LowerStep(GraphicsPreset::Low).has_value());
    CHECK_FALSE(Hint::LowerStep(GraphicsPreset::Auto).has_value());
    CHECK_FALSE(Hint::LowerStep(GraphicsPreset::Custom).has_value());
    CHECK(Hint::TargetIsClassicLook(GraphicsPreset::Low));
    CHECK_FALSE(Hint::TargetIsClassicLook(GraphicsPreset::Medium));
}

TEST_CASE("no hint for Custom, Auto, Low, a kept preset, Never or a stopped session [render][lowfps]")
{
    Hint::PolicyState state;
    state.current = GraphicsPreset::High;
    CHECK(Hint::Target(state) == GraphicsPreset::Medium);

    for (const GraphicsPreset none : {GraphicsPreset::Custom, GraphicsPreset::Auto, GraphicsPreset::Low})
    {
        Hint::PolicyState other = state;
        other.current = none;
        CHECK_FALSE(Hint::Target(other).has_value());
    }

    Hint::PolicyState kept = state;
    kept.kept = GraphicsPreset::High;
    CHECK_FALSE(Hint::Target(kept).has_value());
    // Kept on another preset: this one still gets the hint.
    kept.kept = GraphicsPreset::Ultra;
    CHECK(Hint::Target(kept) == GraphicsPreset::Medium);

    Hint::PolicyState never = state;
    never.enabled = false;
    CHECK_FALSE(Hint::Target(never).has_value());

    Hint::PolicyState stopped = state;
    stopped.stoppedForSession = true;
    CHECK_FALSE(Hint::Target(stopped).has_value());
}

TEST_CASE("a step that gains under 15 % didn't help [render][lowfps]")
{
    CHECK(Hint::StepHelped(20.0f, 23.0f));
    CHECK(Hint::StepHelped(20.0f, 30.0f));
    CHECK_FALSE(Hint::StepHelped(20.0f, 22.9f));
    CHECK_FALSE(Hint::StepHelped(20.0f, 18.0f));
    CHECK(Hint::StepHelped(0.0f, 10.0f));
}
