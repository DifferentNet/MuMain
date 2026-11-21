#pragma once

namespace UI::Chat
{
// How many of a chat list's oldest lines go before a message's rows are added to it: `dropped` at once when it
// holds `most` lines or more, else none. Asked only where rows are really added (CChatLogWindow::AddRows()): a
// message the filter leaves out, or one waiting until the log can measure its rows, evicts nothing.
constexpr int LinesDroppedAtCap(int lines, int most, int dropped)
{
    return lines >= most ? dropped : 0;
}
} // namespace UI::Chat
