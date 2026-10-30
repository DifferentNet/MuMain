// The browser page around a game played on a phone or tablet (the phone view or the touch controls):
// the screen stays on in the world, where a still screen would dim while the MU Helper hunts, and the
// phone's back gesture is the game's Back (UI::HUD::TouchControls::PressBack) instead of leaving the
// page. Web build only; elsewhere it does nothing.
#pragma once

namespace UI::Mobile::PhonePage
{
// Once a frame, between frames.
void Update();
} // namespace UI::Mobile::PhonePage
