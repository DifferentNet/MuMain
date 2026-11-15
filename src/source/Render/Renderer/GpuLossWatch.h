#pragma once

// Tells a lost GPU device (driver reset, device removed) from a frame that failed now and then.
// After a driver reset SDL_gpu keeps refusing work: no command buffer, no swapchain texture, a failed
// submit, frame after frame, while the game itself runs on with a frozen picture. A frame the GPU took
// resets the count; a minimized window (no swapchain texture, no error) is no failure.

#include <cstdint>

namespace Render::Renderer
{

class GpuLossWatch
{
public:
    // Both have to be reached: frames, so a slow frame rate can't fire it after a few hitches, and
    // milliseconds, so a high frame rate can't fire it within a fraction of a second.
    static constexpr std::uint32_t kFailedFramesForLoss = 30;
    static constexpr std::uint64_t kFailedMillisecondsForLoss = 3000;

    // A frame the GPU refused. `nowMs` is any monotonic clock in milliseconds. Returns true once, on the
    // failure that makes the device count as lost.
    bool OnFailure(std::uint64_t nowMs)
    {
        if (m_failedFrames == 0)
            m_firstFailureMs = nowMs;
        if (m_failedFrames < UINT32_MAX)
            ++m_failedFrames;
        if (m_reported)
            return false;
        if (m_failedFrames >= kFailedFramesForLoss && nowMs - m_firstFailureMs >= kFailedMillisecondsForLoss)
        {
            m_reported = true;
            return true;
        }
        return false;
    }

    // A frame the GPU took.
    void OnSuccess()
    {
        m_failedFrames = 0;
        m_reported = false;
    }

    // Whether this failure is worth a log line: the first of a run, then every 100th, so a lost device
    // doesn't write a line a frame.
    [[nodiscard]] bool ShouldLog() const { return m_failedFrames <= 1 || m_failedFrames % 100 == 0; }

    [[nodiscard]] std::uint32_t FailedFrames() const { return m_failedFrames; }

private:
    std::uint32_t m_failedFrames = 0;
    std::uint64_t m_firstFailureMs = 0;
    bool m_reported = false;
};

} // namespace Render::Renderer
