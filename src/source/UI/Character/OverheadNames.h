#pragma once

// MU Moldova: names over heads. One line "[Guild] Name" over every player on screen (and, if
// asked, over the NPCs), drawn by the name window under the hover/chat plates. A character
// whose plate is up (hovered, speaking) shows the plate instead. Options: Interface tab,
// config.ini [UI] OverheadPlayerNames / OverheadNpcNames.
namespace UI::OverheadNames
{
// At most this many names a frame, the nearest to the hero first.
inline constexpr int MaxNames = 40;

void Render();
} // namespace UI::OverheadNames
