#pragma once

// Web build only. The game's canvas as the page shows it, for sizes that follow the real screen
// rather than the game's resolution (UI::Mobile::TouchUnit); and what a phone's page does for the
// game (UI::Mobile::PhonePage): the screen kept on, the back gesture as the game's Back.
namespace mu::web
{
struct CanvasOnPage
{
    // The canvas's size on the page, in CSS pixels.
    float cssWidth = 0.f;
    float cssHeight = 0.f;
    // The game's window pixels in one CSS pixel (the canvas's own pixels over its size on the page).
    float pixelsPerCss = 1.f;
};

// False while the canvas is not laid out (hidden, or no size yet).
[[nodiscard]] bool MeasureCanvas(CanvasOnPage& out);

// Keeps the screen from dimming while on (the browser's screen wake lock), and takes it again when the
// page comes back into view. Nothing where the browser has no wake lock. Cheap to call again.
void KeepScreenOn(bool on);

// While armed, the page keeps a history entry of its own on top for the back gesture (or a phone's
// back button) to step back from: the page stays, and the step counts as a press.
void ArmBackGesture(bool armed);
// Back presses since the last call.
[[nodiscard]] int TakeBackPresses();
} // namespace mu::web
