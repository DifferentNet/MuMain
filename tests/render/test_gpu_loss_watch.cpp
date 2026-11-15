#include "doctest.h"

#include "Render/Renderer/GpuLossWatch.h"

using Render::Renderer::GpuLossWatch;

namespace
{
// Fails `frames` frames, `stepMs` apart from `startMs`; returns how many of them reported the loss.
int FailFrames(GpuLossWatch& watch, int frames, std::uint64_t stepMs, std::uint64_t startMs = 1000)
{
    int reports = 0;
    for (int i = 0; i < frames; ++i)
        reports += watch.OnFailure(startMs + static_cast<std::uint64_t>(i) * stepMs) ? 1 : 0;
    return reports;
}
} // namespace

TEST_CASE("GpuLossWatch: a few failed frames are no loss")
{
    GpuLossWatch watch;
    CHECK(FailFrames(watch, 29, 200) == 0); // 29 frames over 5.6 s: too few frames
    watch.OnSuccess();
    CHECK(FailFrames(watch, 1000, 2) == 0); // 1000 frames in 2 s: too short
}

TEST_CASE("GpuLossWatch: 30 frames over 3 s are a loss, reported once")
{
    GpuLossWatch watch;
    // 60 fps: frame 30 is at 0.48 s, so the loss comes with the frame at 3 s (index 188).
    CHECK(FailFrames(watch, 400, 16) == 1);
    CHECK(watch.FailedFrames() == 400);

    GpuLossWatch slow;
    // 5 fps: 3 s are reached at frame 16, the loss with the 30th frame.
    CHECK(FailFrames(slow, 29, 200) == 0);
    CHECK(slow.OnFailure(1000 + 29 * 200));
}

TEST_CASE("GpuLossWatch: a frame the GPU took starts the count over")
{
    GpuLossWatch watch;
    CHECK(FailFrames(watch, 100, 25) == 0); // 2.5 s
    watch.OnSuccess();
    CHECK(watch.FailedFrames() == 0);
    CHECK(FailFrames(watch, 100, 25, 5000) == 0);
    CHECK(FailFrames(watch, 30, 25, 7500) == 1); // now 3 s of one run
    watch.OnSuccess();
    CHECK(FailFrames(watch, 400, 16, 20000) == 1); // reported again after a recovery
}

TEST_CASE("GpuLossWatch: logs the first failure and then every 100th")
{
    GpuLossWatch watch;
    int logged = 0;
    for (int i = 0; i < 1000; ++i)
    {
        watch.OnFailure(static_cast<std::uint64_t>(i));
        logged += watch.ShouldLog() ? 1 : 0;
    }
    CHECK(logged == 11); // 1, 100, 200, ... 1000
}
