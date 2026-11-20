// The browser page around a game played on a phone or tablet (the phone view or the touch controls):
// the screen stays on in the world, where a still screen would dim while the MU Helper hunts, and the
// phone's back gesture is the game's Back (UI::HUD::TouchControls::PressBack) instead of leaving the
// page. Web build only; elsewhere it does nothing.
#pragma once

namespace UI::Mobile::PhonePage
{
// Once a frame, between frames.
void Update();
// Seconds since the last touch, key or pad press on the page (as of the last Update()), a finger or a
// pad button held down counting as input all along; 0 before the first Update() and outside the web build.
[[nodiscard]] double SecondsSinceInput();
} // namespace UI::Mobile::PhonePage
