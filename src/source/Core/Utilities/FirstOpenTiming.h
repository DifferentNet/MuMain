#pragma once

// The browser client logs what the first opening of a window costs, once per window per session: over
// the opening frame and the two after it, the window's model sync, RmlUi's Context::Update and
// Context::Render, the frames, and the textures loaded meanwhile. Also keeps the last frame's time for
// the window warm-up (UI/Core/WindowWarmUp.h).
#ifdef __EMSCRIPTEN__
namespace Core::FirstOpenTiming
{
// True while a window's first opening is measured; the Add calls cost nothing otherwise.
bool Active();
double NowMs();

// Around the frame (Winmain's RenderProfiledFrame).
void FrameBegin();
void FrameEnd();
// The work of the last frame, from FrameBegin() to FrameEnd().
double LastFrameMs();

void AddRmlUpdate(double ms);
void AddRmlRender(double ms);
// A file RmlUi loaded (RmlUiRenderInterface::LoadTexture); tga: a .tga file (the map's).
void AddRmlTexture(double ms, bool tga);
// A game texture whose deferred load ran (CGlobalBitmap::ResolvePendingLoad).
void AddGameTexture(double ms);

// Around a window's model sync. The first visible one starts the window's measurement.
class SyncScope
{
public:
    SyncScope(const char* window, bool visible);
    ~SyncScope();
    SyncScope(const SyncScope&) = delete;
    SyncScope& operator=(const SyncScope&) = delete;

private:
    int m_window = -1;
    double m_startedAt = 0.0;
};
} // namespace Core::FirstOpenTiming
#endif
