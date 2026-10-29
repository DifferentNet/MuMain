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
    bool hint = false;       // the last row, "F1 : ...": what the next F1 press does
};

// F1 opens the key page and turns to the chat page; one more press closes the window. The
// original also built two further pages (clock, event times) that no key or menu ever reached.
// MU Skynet adds its own keys under the key page and its chat commands under the chat page, then a
// hint row on each.
constexpr int KeyFunctionPage = 0;
constexpr int ChattingPage = 1;
constexpr int PageCount = 2;

std::vector<PageLine> BuildPage(int page);
} // namespace UI::Help
