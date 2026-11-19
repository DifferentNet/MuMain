#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// MU Moldova: the chat log (UI/HUD/ChatLogWindow.cpp) lets old lines go while the chat is closed
// (ghinzuro 2026-11-17: a MU Helper line stayed on screen 20 minutes). Closed means the chat input
// is not open and the log's frame is not shown; then each line stays ChatLineShownMs after it came.
// Open, every line shows again, the whole history as before.
namespace UI::Chat
{
// How long a line stays on screen after it came, while the chat is closed.
inline constexpr std::uint64_t ChatLineShownMs = 30'000;

struct ChatFadeState
{
    // One flag per line, in the order given: true while the line is on screen.
    std::vector<bool> shown;
    // The first line on screen, or -1 when none is.
    int firstShown = -1;
    // When the next line on screen goes (same clock as the times given); empty when none will.
    std::optional<std::uint64_t> nextChangeMs;
};

// Whether a line that came at addedAtMs is on screen at nowMs.
bool IsChatLineShown(std::uint64_t addedAtMs, std::uint64_t nowMs, bool chatOpen);

// Which lines are on screen at nowMs, and when that changes next. Times are milliseconds of one
// monotonic clock.
ChatFadeState EvaluateChatFade(const std::vector<std::uint64_t>& addedAtMs, std::uint64_t nowMs, bool chatOpen);

// The system messages at the top left (CSystemLogWindow) go the same way (ghinzuro 2026-11-17: «Да,
// тоже 30 с»): how many of the newest lines show at nowMs -- at most maxLines, none older than stayMs
// (arrivals oldest first). An open chat (as CChatLogWindow::IsChatOpen counts it) shows the newest
// maxLines whatever their age.
int SystemLinesShown(const std::vector<std::uint64_t>& arrivalsMs, std::uint64_t nowMs, int maxLines,
                     std::uint64_t stayMs, bool chatOpen);

// Round 25: the same system message again within SystemLineRepeatMs of the newest line (its time is
// refreshed on every repeat) adds "×N" to that line instead of a new row.
inline constexpr std::uint64_t SystemLineRepeatMs = 3'000;

// Whether a message that came at nowMs folds into the newest line, which came (or last repeated) at
// newestArrivalMs; sameAsNewest says whether it has the newest line's text and type.
bool FoldsIntoNewestSystemLine(bool sameAsNewest, std::uint64_t newestArrivalMs, std::uint64_t nowMs);

// The line as shown: the text, then " ×N" when it came N > 1 times in a row.
std::wstring WithRepeatCount(const std::wstring& text, int count);
} // namespace UI::Chat
