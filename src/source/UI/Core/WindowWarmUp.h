#pragma once

// The browser client's window warm-up. A window's first opening on a phone took a few hundred
// milliseconds: RmlUi's first layout of its document, the images it loads and its first geometry. Some
// seconds after the hero enters the world, while the frames stay light, the windows the player opens most
// (kWarmUpWindows in the .cpp) are shown unseen for one RmlUi frame each (UI::RmlBridge::InvisibleShow), so
// that work is done before the player opens them. Their windows stay closed for the game all along.
//
// The documents live the whole session, so a window is warmed once per session, the map too: a new
// world's picture loads when the map opens there. Off with config.ini [UI] WarmUpWindows=0.
#ifdef __EMSCRIPTEN__
namespace UI::WarmUp
{
// Once a game frame, before it runs: picks the window to warm in this frame, if any.
void Tick(double lastFrameMs);
// Around the frame's Context::Update() and Context::Render() (RmlUiRuntimeHooks beforeFrame and
// afterFrame). AfterRmlRender() is true when it hid a window again.
void BeforeRmlUpdate();
bool AfterRmlRender();
} // namespace UI::WarmUp
#endif
