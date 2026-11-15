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
    // A run where nothing said the device is gone (only the swapchain refused, with another error) may be a
    // state Windows comes back from (lock screen, session switch, resume): it has to last longer.
    static constexpr std::uint64_t kUnclearFailedMillisecondsForLoss = 30000;

    // A frame the GPU refused. `nowMs` is any monotonic clock in milliseconds; `deviceGone` whether this
    // failure itself says the device is gone. Returns true once, on the failure that makes the device
    // count as lost.
    bool OnFailure(std::uint64_t nowMs, bool deviceGone = true)
    {
        if (m_failedFrames == 0)
        {
            m_firstFailureMs = nowMs;
            m_deviceGone = false;
        }
        if (m_failedFrames < UINT32_MAX)
            ++m_failedFrames;
        m_deviceGone = m_deviceGone || deviceGone;
        if (m_reported)
            return false;
        const std::uint64_t needed = m_deviceGone ? kFailedMillisecondsForLoss : kUnclearFailedMillisecondsForLoss;
        if (m_failedFrames >= kFailedFramesForLoss && nowMs - m_firstFailureMs >= needed)
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
    bool m_deviceGone = false;
};

// Whether SDL's error text names a lost device (Vulkan VK_ERROR_DEVICE_LOST, D3D12 DXGI_ERROR_DEVICE_REMOVED /
// _HUNG / _RESET or their system messages).
[[nodiscard]] inline bool ErrorSaysDeviceGone(const char* error)
{
    if (error == nullptr)
        return false;
    static constexpr const char* kWords[] = {"device_lost", "device lost", "device_removed", "device_hung",
                                             "device_reset", "removed", "will not respond", "suspended"};
    for (const char* word : kWords)
    {
        for (const char* at = error; *at != '\0'; ++at)
        {
            const char* a = at;
            const char* w = word;
            while (*w != '\0' && *a != '\0' &&
                   (*a == *w || (*a >= 'A' && *a <= 'Z' && *a - 'A' + 'a' == *w)))
            {
                ++a;
                ++w;
            }
            if (*w == '\0')
                return true;
        }
    }
    return false;
}

} // namespace Render::Renderer
