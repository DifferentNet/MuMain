// What the on-screen controls of phones and tablets do in the world (UI/HUD/TouchControls): the
// joystick walks, the attack button fights the nearest monster, a skill button casts its hotkey
// slot's skill, the pickup button gathers the drops around. Built on GameLogic::Automation, the same
// primitives the MU Helper and the control socket use, at the helper's own pace: the server reads a
// walk packet every frame as a speed hack.
#pragma once

namespace GameLogic::Touch
{
// Walks the way the joystick points, a few tiles ahead, re-planning as the walk runs out or the
// direction turns. The direction is on the screen (x right, y down); the camera's turn is taken
// into account. Called every frame while the joystick is held.
void Steer(float screenX, float screenY);
// The joystick let go: the character stops at the tile it reached.
void StopSteering();

// Called every frame while the attack button is held: a plain attack on the monster the player
// tapped, else the one fought last, else the nearest.
void Attack();

// Called when a skill button is pressed: its hotkey slot's skill (0-4, the bar's five slots)
// becomes the current skill, as a click on the slot makes it.
void SelectHotKeySlot(int slot);
// Called every frame while a skill button is held: casts that skill, at a monster as Attack()
// picks one, or on the character itself when it is a buff or a heal.
void CastHotKeySlot(int slot);
// Puts the current skill (the bar's current-skill icon, picked in the skill list) on a hotkey slot
// of the bar (0-4), as Ctrl and the slot's digit do on a keyboard, and saves the hotkeys at once: a
// closed browser tab is a logout the client never sees. False when there is no skill to put or it
// is there already.
bool BindHotKeySlot(int slot);

// Called every frame while the pickup button is held: walks to the nearest drop and takes it.
void PickUpNearest();

// Forgets the monster being fought: a new map, the character's death.
void Reset();
} // namespace GameLogic::Touch
