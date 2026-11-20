// The arrow keys walk the hero (pad round 7, C1), as the pad's left stick does: up on the screen is
// up, a diagonal of two arrows walks the slant (Core/Input/ArrowSteer.h). No option row: the arrows
// had no other use in the world.
//
// Only in the world with nothing else wanting the keys: no text field typed in (the chat keeps its
// history on up and down), no game window or dialog open, no control holding the keys (a list, a
// slider), the pad not in use, no other device walking the hero (the touch stick), the hero alive and
// no Ctrl, Alt or Shift held. A walk cut short by one of these, or by a gate, waits until the arrows
// are let go (GameLogic::Touch::PauseSteering), and arrows pressed for something else (the chat's
// caret, a list) walk only once let go and pressed again after it closed. The MU Helper is told the player steers, as with the
// stick. Debug builds keep the arrows for the editor's hero move (ENABLE_EDIT2), the editor's free
// camera keeps them too.
#pragma once

namespace UI::HUD::ArrowWalk
{
// Once a frame after the scene moved, with the pad's: reads the arrows and whether they may walk.
void Update();
// Once a frame after the touch controls acted, with the pad's walk: the walk itself, so a swing or a
// cast that stands the hero holds this frame's walk off.
void LateUpdate();
} // namespace UI::HUD::ArrowWalk
