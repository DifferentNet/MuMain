#pragma once

#include <cstdint>
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
    bool hint = false;       // the last row: what turns the page ("F1 : ...", LB and RB, a tap on it)
};

// The devices the player has in hand, which the first pages are about.
struct Devices
{
    // Touch controls on (UI::Mobile::IsTouchControls()): a phone has no F keys to read about.
    bool touchControls = false;
    // A gamepad in use (UI::Mobile::IsGamepadInUse()).
    bool gamepad = false;
};

// F1 opens the first page and turns to the next; past the last page it closes the window. The
// original had the key page and the chat page (and built two more, clock and event times, that no
// key or menu ever reached). MU Skynet adds its own keys under the key page and its chat commands
// under the chat page, the touch controls and the gamepad's buttons each on a page of their own in
// place of the keys (both on one page are too tall for a phone), and a hint row on each page. With
// touch controls or a gamepad the chat commands get a page of their own as well: the chat page and
// its commands are too tall for a phone too.
enum class Page : std::uint8_t
{
    Keys,
    Touch,
    Gamepad,
    Chatting,
    Commands,
};

// The pages in the order F1 turns them, for the devices in hand: the touch controls, then the
// gamepad, or the keys with neither; then the chat page, and the chat commands' page after it with
// touch controls or a gamepad.
[[nodiscard]] std::vector<Page> Pages(Devices devices);

// The rows of the page at `index` among Pages(devices); past the last, the last.
[[nodiscard]] std::vector<PageLine> BuildPage(int index, Devices devices);
} // namespace UI::Help
