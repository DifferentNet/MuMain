#pragma once

// MU Moldova: names over heads. One line "[Guild] Name" over every player on screen (and, if
// asked, over the NPCs), drawn by the name window under the hover/chat plates. A character
// whose plate is up (hovered, speaking) shows the plate instead. Options: Interface tab,
// config.ini [UI] OverheadPlayerNames / OverheadNpcNames.
class CHARACTER;

namespace UI::OverheadNames
{
// At most this many names a frame, the nearest to the hero first.
inline constexpr int MaxNames = 40;
// Of those, the nearest this many title plates get their effects (UI::TitlePlates::Effects); the rest are drawn still.
inline constexpr int MaxEffectPlates = 8;

void Render();

// An offline player the server shows in town (buff 206, eBuff_OfflineShadow): no menu, trade, party or duel on it.
bool IsOfflineShadow(const CHARACTER* c);
} // namespace UI::OverheadNames
