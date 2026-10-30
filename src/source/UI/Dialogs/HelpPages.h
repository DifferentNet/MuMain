#pragma once

#include <string>
#include <vector>

// The help window's pages (F1), as the original's CHelpWindow::Render() filled the shared
// TextList before drawing it with RenderTipTextList(). The text stays localised through I18N and
// is handed to RmlUi whole, one row per entry.
namespace UI::Help
{
struct PageLine
{
    std::wstring text;
    bool heading = false;    // TEXT_COLOR_BLUE and bold
    bool halfSpacer = false; // the "\n" row: half a text height, no text
};

// F1 opens the key page and turns to the chat page; one more press closes the window. The
// original also built two further pages (clock, event times) that no key or menu ever reached.
constexpr int KeyFunctionPage = 0;
constexpr int ChattingPage = 1;
constexpr int PageCount = 2;

// The devices the player has in hand, which the key page is about.
struct Devices
{
    // Touch controls on (UI::Mobile::IsTouchControls()): a phone has no F keys to read about.
    bool touchControls = false;
    // A gamepad in use (UI::Mobile::IsGamepadInUse()).
    bool gamepad = false;
};

// The key page lists the touch controls, then the gamepad's buttons, for the devices in hand; the
// keyboard's keys with neither.
std::vector<PageLine> BuildPage(int page, Devices devices);
} // namespace UI::Help
