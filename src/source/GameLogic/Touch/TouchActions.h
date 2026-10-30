// What the on-screen controls of phones and tablets do in the world (UI/HUD/TouchControls), and the
// gamepad's buttons with them: the joystick walks, the attack button fights the nearest monster (or a
// player, where players fight or when aimed at), a skill button casts its hotkey's skill, the pickup
// button gathers the drops around. Built on
// GameLogic::Automation, the same primitives the MU Helper and the control socket use, at the
// helper's own pace: the server reads a walk packet every frame as a speed hack.
//
// The joystick and the buttons share the character. A walk breaks off the fight, so the engine's
// own auto-attack stops chasing the monster against the stick; a button that has to stand still
// (a buff, a heal, a cast where the character stands) halts the walk for its swing; the buttons
// fight only what is in reach while the stick walks, instead of walking off towards it.
#pragma once

namespace GameLogic::Touch
{
// Whether a button's action is done (it went off, or cannot: no monster around, a safe zone, a
// refusal) or still pending (out of reach and on its way, the swing before it still playing, a
// cooldown). A tap carries a pending action on (UI::Mobile::FollowThrough).
enum class Progress
{
    Done,
    Pending,
};

// Walks the way the joystick points, a few tiles ahead, re-planning as the walk runs out or the
// direction turns. The direction is on the screen (x right, y down); the camera's turn is taken
// into account. Called every frame while the joystick is pushed. A push held through a gate, a
// map change or controls that went away does not walk on the far side: it waits until let go.
void Steer(float screenX, float screenY);
// The joystick let go (or back in its centre): the character finishes its step and stands on the
// tile it was stepping onto, and the next push walks again.
void StopSteering();
// The controls went away under a pushed joystick (a map change, a dialog, death): the walk stops,
// and the push waits until let go, as after a gate.
void PauseSteering();
// Whether the joystick is pushed but waiting to be let go.
[[nodiscard]] bool IsSteeringOnHold();
// Whether the joystick walks the character now.
[[nodiscard]] bool IsSteering();

// Whether the pointer is the gamepad's: then a monster under it is the one to fight. A finger's
// tap leaves the pointer on whatever it touched last, which is no aim.
void SetPointerAims(bool aims);

// The PK switch of the menu: while on, the buttons fight a player a tap or the pad's pointer aimed
// at, and a tap on a player attacks, as Ctrl held lets a click (CheckAttack). A duel, Chaos Castle and
// the other places where players fight need no switch. Never a member of the party. Off again on a
// new map.
void SetForceAttack(bool on);
[[nodiscard]] bool IsForceAttack();

// The party member a tap on their party card chose (CPartyListWindow): the heal and buff buttons
// cast on them while they are near, else on the character. A tap on the same card again, or on the
// character's own, goes back to the character. FriendKey is NoTarget (-1) while none is chosen or
// the one chosen is away.
void ToggleFriend(int characterKey);
[[nodiscard]] int FriendKey();

// Keeps the player the engine fights (a tap on them) as the buttons' target before the finger moves
// on to a button, where the selection under the pointer goes. Called every frame.
void TrackAim();

// A plain attack on the monster or player aimed at, else the one fought last, else the nearest
// monster (or enemy player where players fight). Called every frame while the attack button is held,
// and after a tap until it is done.
Progress Attack();

// A press on a skill button: its hotkey's (0-9) skill becomes the current skill, as a click on the
// bar's slot makes it.
void SelectHotKey(int hotKey);
// Casts the hotkey's skill, at a monster as Attack() picks one, or on the character itself when it
// is a buff, a heal or a summon (a heal or a buff on the party member chosen, ToggleFriend). Hellfire
// and Inferno walk up to the monster first. A skill short of mana or AG tells so and asks for a mana
// potion, leaving the turn to the attack button. A pet command goes to the raven at once. Skills
// aimed where the player points only become the current skill (SelectHotKey). Called every frame
// while a skill button is held, and after a tap until done.
Progress CastHotKey(int hotKey);
// Puts the current skill (the bar's current-skill icon, picked in the skill list) on a hotkey, as
// Ctrl and the digit do on a keyboard, and saves the hotkeys at once: a closed browser tab is a
// logout the client never sees. False when there is no skill to put or it is there already.
bool BindHotKey(int hotKey);
// Whether a hotkey holds a skill or a pet command the character can use now: not an empty one, a
// pet command without a pet, or a skill the character no longer has (another character's hotkeys).
[[nodiscard]] bool HotKeyHoldsSkill(int hotKey);

// Walks to the nearest drop that fits the bag and takes it, then the next, until none is left; says
// so once in a while when only drops that do not fit lie around. Called every frame while the
// pickup button is held, and after a tap until done.
Progress PickUpNearest();

// Forgets the monster being fought, the drops passed over, the PK switch and the party member chosen,
// and makes a pushed joystick wait: a new map, the character's death, the controls hidden.
void Reset();
} // namespace GameLogic::Touch
