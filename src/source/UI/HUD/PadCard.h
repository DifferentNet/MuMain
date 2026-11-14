// The gamepad's controls card (pad round 3, B4): every button and what it does, a page for the world
// and one for the windows, drawn in pad_overlay.rml with the glyphs of the pad in hand. It shows once
// by itself, at the first press in a safe zone ([UI] GamepadCardSeen), and opens from the hero hub's
// Controls tab. UI/HUD/GamepadControls plays it (LB and RB or the d-pad's sides turn the page; B, A or
// Menu closes it); UI/HUD/PadOverlay draws it.
#pragma once

namespace UI::HUD::PadCard
{
inline constexpr int PageCount = 2;
inline constexpr int WorldPage = 0;
inline constexpr int WindowsPage = 1;

// Opens it on its first page.
void Open();
void Close();
[[nodiscard]] bool IsOpen();
// The page `step` pages on, round the ends.
void Turn(int step);
[[nodiscard]] int Page();
} // namespace UI::HUD::PadCard
