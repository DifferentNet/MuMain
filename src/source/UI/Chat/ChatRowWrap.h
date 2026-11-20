#pragma once

#include <functional>
#include <string>
#include <vector>

// MU Skynet: the chat log's rows (UI/HUD/ChatLogWindow.cpp, CChatLogWindow::SplitIntoRows()). A line of the log
// never wraps on its own (chat_log.rcss: white-space: nowrap; each row is a model entry the pointed-line test and
// the scroll count), so a message wider than the log is split into as many rows as it needs when it comes: the
// first row is drawn as "ID : text", the rows after it as the text alone.
namespace UI::Chat
{
// Room kept free at the end of a row for the font drawing a little wider than it measured: this share of the
// row's width, at least MinRowMarginPx.
inline constexpr float RowMarginShare = 0.025f;
inline constexpr float MinRowMarginPx = 2.f;

// The width a row's text may take in a row `room` wide, the margin above kept free.
float ChatRowWidth(float room);

// How wide a row is drawn, in the unit the row width is given in.
using MeasureChatRow = std::function<float(const std::wstring& row)>;

// The narrowest row worth splitting a message for: a row narrower than these few wide letters would cut a
// message into rows of a character or two, so the log splits it the original way instead.
inline constexpr wchar_t MinRowSample[] = L"WWWWW";
bool ChatRowTooNarrow(float rowWidth, const MeasureChatRow& measure);

// What a message is split into rows with when it comes (CChatLogWindow::CanSplitRows()).
enum class ChatRowSource
{
    // Not yet: RmlUi has not updated the log's document since it was built, or not laid out its hidden row; the
    // message waits for the next frame.
    Wait,
    // The hidden row of the message's type (chat_log.rml), in the log's own font.
    LogFont,
    // The native font against the original's client width: no RmlUi at all, or a log whose chat_log.rml has no
    // hidden row (an older or overridden theme), which would otherwise keep every message waiting.
    NativeFont,
};
// `rmlUi`: RmlUi runs; `logUpdated`: RmlUi has updated the log's document since it was built; `hasProbe`: that
// document has the hidden row of the message's type; `room`: the width that row gives a row's text, 0 until laid out.
ChatRowSource ChooseChatRowSource(bool rmlUi, bool logUpdated, bool hasProbe, float room);

// The rows `text` takes when no row may be wider than `rowWidth`. The first row is drawn with `prefix`
// ("ID : ") in front of it and counts it as its first word: a word that does not fit after it starts the
// second row, and the first holds the sender alone. The rows hold the text only. A row breaks at a space (the
// spaces at the break go); only a word wider than a whole row breaks between two characters. Every row but a
// sender's own holds at least one character, so a row too narrow for any still comes to an end; with a prefix
// wider than a row, the first row takes one character after it. One empty row for an empty text.
std::vector<std::wstring> WrapChatRows(const std::wstring& prefix, const std::wstring& text, float rowWidth,
                                       const MeasureChatRow& measure);
} // namespace UI::Chat
