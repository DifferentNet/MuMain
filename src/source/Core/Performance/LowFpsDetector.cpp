#include "Core/Performance/LowFpsDetector.h"

#include <algorithm>

namespace Core::Performance
{
namespace
{
constexpr float MillisecondsPerSecond = 1000.0f;
} // namespace

LowFpsDetector::LowFpsDetector(LowFpsRules rules) : m_rules(rules)
{
}

float LowFpsDetector::SlowBarFps(const LowFpsRules& rules, double capFps)
{
    if (capFps <= 0.0)
        return rules.slowFps;
    const float capBar = static_cast<float>(capFps) * rules.capShare;
    return std::min(rules.slowFps, capBar);
}

void LowFpsDetector::StartWarmup(double nowMs)
{
    ClearAll();
    m_measureFromMs = nowMs + m_rules.warmupMs;
}

void LowFpsDetector::StartQuiet(double nowMs)
{
    ClearAll();
    m_measureFromMs = std::max(m_measureFromMs, nowMs + m_rules.quietMs);
}

void LowFpsDetector::Reset()
{
    ClearAll();
    m_measureFromMs = 0.0;
}

void LowFpsDetector::ClearWindow()
{
    // Called every frame during the warm-up and the quiet time: an empty histogram needs no clearing.
    if (m_framesInWindow > 0)
        m_histogram.fill(0);
    m_framesInWindow = 0;
    m_windowOpen = false;
}

void LowFpsDetector::ClearAll()
{
    ClearWindow();
    m_slowWindows = 0;
}

float LowFpsDetector::MedianFrameMs() const
{
    // The middle frame of the sorted window: the first bucket that holds it, at the bucket's lower edge. The edge
    // rather than the middle, so a steady 30 FPS (33.33 ms, bucket 33.25) reads as 30.08 FPS and is not slow: the
    // error (under a quarter of a millisecond) always falls on the side of no hint.
    const std::uint32_t middle = static_cast<std::uint32_t>(m_framesInWindow + 1) / 2;
    std::uint32_t counted = 0;
    for (int bucket = 0; bucket < HistogramBuckets; ++bucket)
    {
        counted += m_histogram[bucket];
        if (counted >= middle)
            return static_cast<float>(bucket) / static_cast<float>(BucketsPerMs);
    }
    return static_cast<float>(HistogramBuckets) / static_cast<float>(BucketsPerMs);
}

LowFpsFrameResult LowFpsDetector::FinishWindow(double capFps)
{
    LowFpsFrameResult result;
    const bool enoughFrames = m_framesInWindow >= m_rules.minFramesPerWindow;
    if (!enoughFrames)
    {
        // Mostly stalls: it says nothing about the frame rate, and the row of slow windows is broken.
        ClearAll();
        return result;
    }

    result.windowCompleted = true;
    // A window of frames all under a quarter of a millisecond has its median at 0: count it as very fast.
    const float medianMs = std::max(MedianFrameMs(), 1.0f / static_cast<float>(BucketsPerMs));
    result.windowMedianFps = MillisecondsPerSecond / medianMs;
    result.windowSlow = result.windowMedianFps < SlowBarFps(m_rules, capFps);
    ClearWindow();

    m_slowWindows = result.windowSlow ? m_slowWindows + 1 : 0;
    if (m_slowWindows >= m_rules.slowWindowsInRow)
    {
        result.hintDue = true;
        m_slowWindows = 0;
    }
    return result;
}

LowFpsFrameResult LowFpsDetector::OnFrame(double nowMs, float frameMs, bool steady, double capFps)
{
    if (!steady)
    {
        ClearAll();
        return {};
    }
    if (nowMs < m_measureFromMs)
    {
        ClearWindow();
        return {};
    }
    m_measureFromMs = 0.0;

    if (!m_windowOpen)
    {
        m_windowOpen = true;
        m_windowStartMs = nowMs;
    }
    const bool stall = frameMs > m_rules.stallFrameMs || frameMs < 0.0f;
    if (!stall)
    {
        const int scaled = static_cast<int>(frameMs * static_cast<float>(BucketsPerMs));
        const int bucket = std::min(scaled, HistogramBuckets - 1);
        ++m_histogram[bucket];
        ++m_framesInWindow;
    }

    if (nowMs - m_windowStartMs < m_rules.windowMs)
        return {};
    return FinishWindow(capFps);
}

} // namespace Core::Performance
