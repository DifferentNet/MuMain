#include "stdafx.h"
#include "Core/Utilities/FirstOpenTiming.h"

#ifdef __EMSCRIPTEN__
#include "Core/Utilities/Log/MuLogger.h"

#include <emscripten.h>

#include <array>
#include <cstring>

namespace Core::FirstOpenTiming
{
namespace
{
// The opening frame and the two after it.
constexpr int kFramesMeasured = 3;
constexpr int kMaxWindows = 8;

struct Loads
{
    int count = 0;
    double ms = 0.0;

    void Add(double elapsed)
    {
        ++count;
        ms += elapsed;
    }

    void Add(const Loads& other)
    {
        count += other.count;
        ms += other.ms;
    }
};

struct Totals
{
    double syncMs = 0.0;
    double rmlUpdateMs = 0.0;
    double rmlRenderMs = 0.0;
    Loads rmlTextures;
    Loads tga;
    Loads gameTextures;

    void AddFrame(const Totals& frame)
    {
        rmlUpdateMs += frame.rmlUpdateMs;
        rmlRenderMs += frame.rmlRenderMs;
        rmlTextures.Add(frame.rmlTextures);
        tga.Add(frame.tga);
        gameTextures.Add(frame.gameTextures);
    }
};

struct Window
{
    const char* name = nullptr;
    // Frames still to measure; 0 before the first opening and after the measurement.
    int framesLeft = 0;
    bool seen = false;
    Totals totals;
    std::array<double, kFramesMeasured> frameMs{};
};

Window g_windows[kMaxWindows];
int g_windowCount = 0;
int g_tracking = 0;
double g_frameStartedAt = 0.0;
double g_lastFrameMs = 0.0;
// The frame's RmlUi and texture work, for every window measured in it.
Totals g_frame;

int FindOrAdd(const char* name)
{
    for (int i = 0; i < g_windowCount; ++i)
    {
        if (std::strcmp(g_windows[i].name, name) == 0)
            return i;
    }
    if (g_windowCount == kMaxWindows)
        return -1;
    g_windows[g_windowCount].name = name;
    return g_windowCount++;
}

void LogWindow(const Window& window)
{
    const Totals& totals = window.totals;
    mu::log::Get("render")->info(
        "First open {} ({} frames): sync {:.1f} ms, rml update {:.1f} ms, render {:.1f} ms, "
        "frames {:.1f} / {:.1f} / {:.1f} ms; rml textures {} in {:.1f} ms (tga {} in {:.1f} ms), "
        "game textures {} in {:.1f} ms",
        window.name, kFramesMeasured, totals.syncMs, totals.rmlUpdateMs, totals.rmlRenderMs, window.frameMs[0],
        window.frameMs[1], window.frameMs[2], totals.rmlTextures.count, totals.rmlTextures.ms, totals.tga.count,
        totals.tga.ms, totals.gameTextures.count, totals.gameTextures.ms);
}

void EndWindowFrame(Window& window, double frameMs)
{
    window.totals.AddFrame(g_frame);
    window.frameMs[kFramesMeasured - window.framesLeft] = frameMs;
    if (--window.framesLeft > 0)
        return;
    --g_tracking;
    LogWindow(window);
}
} // namespace

bool Active()
{
    return g_tracking > 0;
}

double NowMs()
{
    return emscripten_get_now();
}

void FrameBegin()
{
    g_frameStartedAt = NowMs();
}

void FrameEnd()
{
    g_lastFrameMs = NowMs() - g_frameStartedAt;
    if (Active())
    {
        for (int i = 0; i < g_windowCount; ++i)
        {
            if (g_windows[i].framesLeft > 0)
                EndWindowFrame(g_windows[i], g_lastFrameMs);
        }
    }
    g_frame = {};
}

double LastFrameMs()
{
    return g_lastFrameMs;
}

void AddRmlUpdate(double ms)
{
    g_frame.rmlUpdateMs += ms;
}

void AddRmlRender(double ms)
{
    g_frame.rmlRenderMs += ms;
}

void AddRmlTexture(double ms, bool tga)
{
    g_frame.rmlTextures.Add(ms);
    if (tga)
        g_frame.tga.Add(ms);
}

void AddGameTexture(double ms)
{
    g_frame.gameTextures.Add(ms);
}

SyncScope::SyncScope(const char* window, bool visible)
{
    if (!visible)
        return;
    const int index = FindOrAdd(window);
    if (index < 0)
        return;
    Window& entry = g_windows[index];
    if (!entry.seen)
    {
        entry.seen = true;
        entry.framesLeft = kFramesMeasured;
        ++g_tracking;
    }
    if (entry.framesLeft == 0)
        return;
    m_window = index;
    m_startedAt = NowMs();
}

SyncScope::~SyncScope()
{
    if (m_window >= 0)
        g_windows[m_window].totals.syncMs += NowMs() - m_startedAt;
}
} // namespace Core::FirstOpenTiming
#endif
