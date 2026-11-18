#pragma once

#include <cstdint>

// The party list carries each member's map and tile, but the server sends it only when the party changes or when it
// is asked. While the big map or the party window shows the members, it is asked every 5 seconds; both open still
// ask once per 5 seconds.
namespace UI::Party::ListRefresh
{
// Once a frame from each window that shows the members; `wanted` = the window is open and there is a party.
void Tick(bool wanted);

// The list was just asked elsewhere (the party window asks when it opens).
void NoteAsked();

// Whether the party list should be asked now: something wants it (the map or the party window is open and there is
// a party) and the last request is at least `intervalMs` old. The caller notes the request with the time it sent it.
[[nodiscard]] inline bool RefreshDue(bool wanted, bool askedBefore, std::uint32_t lastAskedMs, std::uint32_t nowMs,
                                     std::uint32_t intervalMs)
{
    if (!wanted)
        return false;
    return !askedBefore || nowMs - lastAskedMs >= intervalMs;
}
} // namespace UI::Party::ListRefresh
