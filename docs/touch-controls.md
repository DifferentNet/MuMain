# Touch controls (browser client on phones and tablets)

On a touch device the browser client (/play) shows on-screen controls laid out as MU's mobile games
have them, and routes fingers itself instead of letting SDL turn the first finger into the mouse,
and it switches to the phone view (below). Two rows in Options → Interface choose this, saved in
config.ini `[UI]`:

| Row | Key | Choices |
|---|---|---|
| Interface view | `InterfaceView` | Auto, PC, Phone |
| Controls | `Controls` | Auto, Mouse and keyboard, Touch |
| Button size (only while the on-screen controls show) | `TouchButtonSize` | 80, 90, 100, 115, 130 % |
| Gamepad ([below](#gamepad-xbox-layout)) | `Gamepad` | On, Off |

Auto means the phone view and touch controls on a touch device (`mu::web::IsTouchDevice()`), the PC
view and the mouse elsewhere. A desktop build or a computer's browser is unchanged unless the player
picks Phone or Touch (a Windows touch screen works with Touch). The touch menu's View item switches
between the PC and phone views at once. In the browser the page's own menu (⋮ on a phone, the
footer on a computer) has a View item too: it steps Auto → PC → Phone and shows the choice in force
(`Core/Platform/web/WebPageView.{h,cpp}`, `window.muView` in `src/web/index.html`; the game takes
the page's request between frames, `UI::Mobile::FollowPage`).

## What the player gets

| Where | Control | What it does |
|---|---|---|
| Bottom left | Joystick (floating: it appears where the thumb lands) | Walks the way it points, a few tiles ahead, re-planning as the walk runs out or the thumb turns. Letting go cuts the planned walk short at the next tile (`Automation::StopAtNextTile`, tested in `tests/gamelogic/test_walk_stop.cpp`): the hero stops within a step instead of finishing the tiles planned ahead. The page going out of sight (`SDL_EVENT_WINDOW_HIDDEN`) lets go of every finger, as a phone sends no lift for a thumb still down when the app is switched. |
| Above the joystick | Q, W | The item hotkeys Q and W (potions). |
| Along the top, right of the location bar | Back, Menu, Inventory, Character, Chat, Teleport, Map | Back is Esc: it closes the top window (or opens the system menu with none open). Menu opens a panel of 21 items under the row (20 for a player who is not a GM): windows, Skills (the skill picker, below), View (PC / phone), Chat (for a pad, which has no Chat button of its own), Player actions (D, the commands for the character under the pointer), the PK switch (below) and the GM panel (Y, GMs only). The others press the window's key (I, C, Enter, M, Tab). |
| Bottom right | Attack (held) | Plain attack on the monster tapped last, else the one fought last, else the nearest within 8 tiles. Where players fight (a duel, Chaos Castle, a Gens enemy on the strife maps) a player tapped or fought counts as a monster does, and the nearest enemy player too. |
| Menu | PK switch | Ctrl for a finger: while on, Attack reads "PK" in red, a tap on a player attacks and the buttons fight the player tapped last, as Ctrl and a click do on a PC (`TouchActions::SetForceAttack`, the engine's `CanAttackCharacter`). Never a member of the party; off again on a new map or at death. |
| Party cards | A tap on a member's card | The skill buttons' heals and buffs go to that member while they are near, and the card stays lit; a tap on it again (or on the character's own) gives them back to the character (`TouchActions::ToggleFriend`). The card's leave button is no pick. |
| Around Attack | The hotkey bar's five skills | A tap makes the skill current (as a click on its slot). Held, it casts: buffs and heals on the character, area buffs around it, the rest at a monster as Attack picks one. Teleport only selects (aim it with a long press). |
| Around Attack | Putting a skill on a button | The skill picker: a panel on the left of the dimmed screen lists the skills (icon and name, as many to a page as fit, < > turn the pages), the five skill buttons stay lit right of it. The button being set glows gold; a tap on another button chooses that one. A tap on a skill puts it there and the next empty button is chosen, so a new character fills the five with five taps. A skill already on another button swaps places with the one it replaces (each skill is on one button at most; a gold dot marks the skills on a button, the chosen button's skill is lit). Remove empties the chosen button, Done, Back or a tap beside the panel close it. It opens from the Skills button beside the arc, from Menu → Skills, or from a tap on an empty skill button (which shows a plus). The hotkeys are saved to the server at once (`SaveOptions`), as a closed tab never logs out. The rules (which hotkey a button is, swapping, the next empty button, how many skills a page holds) are `UI/Mobile/SkillArrange.{h,cpp}`, tested in `tests/ui/test_skill_arrange.cpp`. The game's own skill list (the PC bar's current-skill icon) still binds as before: with it open, a tap on a button and then a skill, or a skill and then a button within 8 s. |
| Left of the skill buttons | Skills | Opens the skill picker on the first empty button (else the first). |
| Right edge | Pick up (held) | Walks to the nearest drop within 6 tiles and takes it. |
| Under the skills | Auto | MU Helper on/off (the Home key); lit while it runs. |
| Anywhere else | One finger = the mouse | Tap = left click (double tap = double click), drag = left button held from where the finger landed, hold 0.5 s = right click (on an item in the bag or worn gear it opens the item card instead, below; not while an item hangs on the pointer, so it can still be dragged), two fingers = pinch zoom (mouse wheel; a second finger that lands while the first drags calls the drag off, and a finger landing again while one of the pinch's is still down pinches on). A tap on the game ends typing (the chat lets go of the keys), as does any key button but Enter and Esc. A drag the browser takes away, or a turn of the screen, drops and clicks nothing. The camera's zoom lock (F10 on a computer, on by default against a stray wheel) is lifted whenever the controls show, as a phone has no F10. |

## Item card

A finger held on an item in the bag opens a card in place of the PC's right click, which would use,
equip, sell or drop the item at once: the item's name, then the right click's action (Move with the
storage, trade or chaos machine open, Sell in a shop), Put on button Q / W (for items the hotkeys
take), Drop (only with the bag alone open, for items that may be dropped; it asks first) and Close.
With the bag alone open the action is what the right click does to that item: Use only for the items
it uses (potions, scrolls, tickets, skill books and the like: `IsUsedFromBag`, kept in step with
`TryConsumeItem`), Equip for gear, Apply… for a jewel worked onto items (it lifts the jewel, and the
next tap on an item in the bag applies it there), Open for a box the server opens where it is dropped
(Box of Luck, the Boxes of Kundun, the event boxes, the SkyNet Box; no Drop beside it), and nothing
for the rest. Each button checks that the same item still lies there before it acts. The pad's R3
opens the same card, except with the storage or the chaos machine open, where it moves the item at
once as before (`CInventoryActionController::AsksWithItemCard`). A mouse's right click is
unchanged. The card
opens at the edge of the screen away from the item, so the item's tooltip beside it stays in view
(`GenericMenuConfig::edge`, the `gmd-edge-left` / `gmd-edge-right` classes of
`generic_menu_dialog.rcss`).

A finger held on a "+" of the character window (a right click, R3 on a pad) opens a menu of bigger
steps for the free points: +10, +100, +1000 while there are more, then all of them, spent through
the server's `/addstr` and the like, which stop at a stat's maximum
(`UI/Character/StatPointsMenu.{h,cpp}`, tested in `tests/ui/test_stat_points_menu.cpp`). A plain tap
still adds one point.

On worn gear (the character's equipment slots) the card has Take off: the item goes to the first
free place in the bag, as a mouse's right click there does, or the system messages say the bag is
full. Lifting the finger takes nothing off by itself (`CMyInventory::ShowWornItemCard`). A tap on worn
gear opens the same card rather than lifting the item onto the finger, unless a window that takes a
held item is open (a shop, the storage, a trade, the chaos machine, a personal shop), where it lifts
it as a click does; a drag lifted over worn gear takes nothing (`HandleFingerReleaseOnWorn`). R3 on
worn gear opens the card too.

In the storage and the extended storage a finger held on an item opens a card with Move to bag (a
right click moves it at once). The chaos machine's long press still moves the item back at once.

In an NPC shop a tap selects an item: its cells light up, its tooltip shows and, once a shop opening,
a line says to tap it again; a second tap on the same item within 1.5 s buys it. A tap on another
item selects that one, and a drag lifted on the grid buys nothing. The mouse and the pad buy with one
click, as before; a gamble shop asks first anyway (`UI/NPCs/ShopTapToBuy.{h,cpp}`, tested in
`tests/ui/test_shop_tap_to_buy.cpp`).

## Phone view

The phone view replaces the PC's bottom bar while the touch controls show: a hero frame top left
(level, HP and MP with their numbers, SD and AG), a thin EXP line along the bottom edge, and how many
potions Q and W hold under the buttons. The PC bar and the top-right buttons hide (the skill picker
stands in for its skill list); the system messages move under the hero frame, the pets'
HP frames right of it, the buffs under the top row and the chat right of the joystick's area. Without touch controls the phone view keeps the PC bar.

The phone view also draws the login box, server list, character select buttons, system menu and
menus larger, for fingers (the character select buttons at twice their art, about 36 CSS px tall on a
phone 360 px high).

Windows, the HUD bar and tooltips take a finger before the controls: a finger on an open window
plays the mouse there even where a button lies under it. A window a workspace slot placed counts by
its panel's box (`#panel.slot-placed`), as the inventory, shops, storage and trade panels take no
pointer events themselves (their item grids are clicked natively) and RmlUi's hit test looks through
them. The chat log, the scrolling notice and the events' timers and scores (Blood and Chaos Castle,
Kanturu, Doppelganger, Empire Guardian, duels, Battle Soccer: class `touch-through` on their root) do
not take the finger: they only show text, over the joystick's corner, the top row and the fight.

The pointer stays where the last tap lifted, so a monster tapped stays Attack's target and an item
tapped shows its tooltip. A tap on a top-row button or a menu item takes it off the window
(`Touch::RestPointer`): a window opening where the last tap was would show the tooltip of whatever
lies there. A pad's pointer is the player's own and stays.

Every action keeps the MU Helper's pace (walk packets 300 ms apart, attacks and skills 250 ms):
the server reads a burst of walk packets as a speed hack.

## Sizes: finger units

The controls are sized for the finger, not for the game's resolution: `touch_controls.rcss` (both
themes) and the pad glyphs it shows (`gamepad_glyphs.rcss`) write their sizes in `rem`, and C++ sets the
document's font size to one *finger unit* (`UI/Mobile/TouchUnit.{h,cpp}`). In the browser a unit is
one CSS pixel on a phone (a 50-unit skill button is 50 CSS px, Google's and Apple's smallest touch
target being 44–48), up to 1.15 on a tablet (the short side over 412 CSS px), times the player's Button
size, but never more than the screen's height holds (the left column of top row, hero frame and
potions over the joystick is 345 units tall): `UI/Mobile/ButtonSize.{h,cpp}`, tested in
`tests/ui/test_button_size.cpp`. It follows the canvas's size on the page, measured again when the window
or the percent changes and every second (the browser's zoom changes it alone). Off the web a unit is
RmlUi's dp at the player's percent. The finger's slop, the stick's reach and its dead zone are in units
too.

Only what lines up with the PC's own HUD stays in dp: the top row starts right of the location bar
(214dp), and the hero frame sits under it (4dp, 26dp, 200dp wide).

The phone's HUD pieces in other documents, which their themes draw in dp, follow the controls'
boxes instead (`UI/Mobile/PhoneHudLayout.{h,cpp}`, from `TouchControls::Update`): the buffs and the
pets' frames under the top row (the pets right of the hero frame), the system messages under the hero
frame (right of the potions where six lines would reach them), and the skill list with its right edge
just left of the skill buttons; the hint for putting a skill on a button stays over the buttons, no
wider than the room right of the list. The themes' `@media (theme: mobile-hud)` places are the first guess
until then; leaving the phone's HUD gives the themes' places back.

The party's cards go under the top row's right end (short of the page's ⋮), the first column on the
right: up to 1.4 times the dock's size so a finger can pick one, in one column or two (a party of
five), smaller in steps down to the dock's size where a column would reach a control round Attack
(`UI/Mobile/PhoneHudFit.{h,cpp}`, tested in `tests/ui/test_phone_hud_fit.cpp`; `party_list.rcss`
wraps the cards). The chat moves left until it clears the skill buttons beside it (on a phone 640
CSS px wide it would reach the first one), never past the screen's left edge. World labels (names,
chat balloons, shop titles) under the top row's band, the hero frame with the location bar, or a
control draw at a quarter of their strength (`PhoneHudLayout::CoversLabel`, used by
`UI/Character/WorldLabelLayer`): they draw under every window, and their text would mix with the
buttons'. The notices go under the open menu and skill picker, dimmed by the backdrop, and back over
every window once they shut (`UI/Mobile/NoticesUnderPanel`). The PC's HUD shows both as before.

In the phone's HUD the top row's buttons are as wide as their labels, 44 to 72 units (a wide phone
looks as before), and the labels get smaller, not under 9 units, where the row is too short for them
at 12 units: Russian's on a phone 640 CSS px wide. Pick up is a pill 62 units wide for "Поднять";
the skill picker's Remove, Done and page buttons are 44 units tall; no text is under 12 units. The
hero frame's SD and AG show their numbers short from 10 000 on ("12k/32k", `UI/Mobile/CompactNumber`,
tested in `tests/ui/test_compact_number.cpp`), HP and MP in full. A PC with a touch screen or a pad
keeps the controls' own sizes.

The phone view also raises the chat's and the system messages' text to at least 12 units at 100 %
(`TouchUnit::ReadableTextPx`), with their lines and rows; the chat's well keeps its height and shows
fewer rows.

## The page on a phone

`UI/Mobile/PhonePage.{h,cpp}` (browser build only; `Core/Platform/web/WebScreen.{h,cpp}` holds the
page's side):

- The screen stays on in the world while the game is played by hand (the phone view or touch
  controls): the browser's screen wake lock, taken again when the page shows after being hidden and
  asked for every 5 s while wanted (a battery saver may refuse it for a while). A phone's screen would
  otherwise dim while the MU Helper hunts. With the helper off, it lets the screen dim after 5 minutes
  with no touch, key, wheel or pad press (`Module.muInput` counts the page's input), as a phone left on
  a table should; the next touch keeps it on again.
- The back gesture (or Android's back button) is the game's Back instead of leaving the page: the page
  keeps a history entry of its own on top (`Module.muBack`), and stepping back from it counts a press
  and puts it back. A press closes the touch menu if it is open, else does what Back does
  (`TouchControls::PressBack`); out of the world it is Esc. Chrome may let a back gesture skip an entry
  pushed without a tap; the next tap on the page restores it.

## Windows in the phone view

Each window restyles itself under `@media (theme: mobile)` in both themes; on a PC nothing changes.

| Windows | What the phone view does |
|---|---|
| Inventory, storage, shops, trade, chaos machine, personal shops | The inventory alone fills the screen (equipment left, bag right, cells about 40 CSS px). Beside another window `#panel` gets the class `beside`: the bag only. The 15-row grids show part of the grid in an `.item-grid-viewport` paged by ▲▼ (`data-grid-scroll`); a drop outside the shown rows is refused. The 4-row windows put the grid right, text and big buttons left. |
| Character, quests, party, friends, battle pass, help, options, teleport | Wider panels (`docked_panel_frame.rcss` repeats the frame's middle so the art stays undistorted), bigger tabs, rows and buttons, a corner close a finger hits (44 × 40 px). |
| MU Helper | The settings 640 units wide in two columns, the pieces a third larger, a check box's finger area running on over its label, and a line saying how to clear a skill slot (hold it; R3 on a pad). Its Setting panel beside it, 300 units wide: each pane the panel's width, the condition radios in one column, long labels on two lines, a whole check row a target, the gauges half as large again, Save and Initialization the panel's width. The skill picker's entries 40 × 46 in columns of eight. The pieces both windows share are in `mu_helper_common.rcss`'s phone block. |
| Commands | A side panel 300 units wide (`chat_command.rcss`): rows 34 units tall, every command listed and scrolled by finger instead of by page; a command's page fits its description, each parameter's name over a 30-unit box, and the actions to the panel (`CChatCommandWindow::CurrentPage`, re-wrapped when the view changes). |
| Skill list (the PC bar's current-skill icon) | Rows of eight finger-sized cells on a dark plate above the controls. Back closes it (Esc would open the system menu over it). |
| Map (Tab) | Full screen, under the touch controls so their Back closes it. |
| Touch menu | Drawn over every window while open (`OpenTouchMenuDepth`); a tap on the dimmed screen around it shuts it, and it shuts while a dialog waits for an answer. |

A finger that moves on a scrollable list scrolls it, as on a phone, rows and buttons included (they
click only on a tap); a flick glides on and a tap on a gliding list only stops it. Item grids,
scrollbars, sliders (RmlUi's `drag` property other than `block`) and text fields keep the drag;
a slider inside a list that scrolls up and down (Options) takes the finger only when it starts
sideways, and an up or down swipe over it scrolls the list (`TouchGesture::IsMostlyVertical`)
(`Render/RmlUi/RmlTouchScroll.{h,cpp}`, reached from `Touch::Pointer` through
`Core::Input::BeginUiTouchScroll` and the other `IUiInputConsumer` touch-scroll calls).

With touch controls on, Help's first page lists the touch controls instead of the F keys, and MU
Skynet's chat commands get a page of their own after the chat page: with them under the chat rows
the page ran past a phone's screen (`UI::Help::Pages(devices)`, `UI::Help::BuildPage(page, devices)`).
A tap on the hint row turns the page; the finger's pointer then leaves the window
(`TouchControls::RestPointerOnLift`), so an inventory under the page shows no tooltip for an item
nobody tapped.

## Gamepad (Xbox layout)

A gamepad plays the game in the browser and the desktop build alike (Windows, a Steam Deck's own
pad): SDL's gamepad layer names the buttons by position, so any pad SDL knows plays as an Xbox pad.
It is on by default: the first button pressed or stick pushed makes the pad the device in use, and
a key, a click or a finger hands the game back. In a browser a pad shows only after its first button
press (the Gamepad API's rule); that press counts as a tap (`mu::web::HeldStandardButtons`), a hold
that began with it from the next press.

Options -> Interface -> Gamepad (`[UI] Gamepad` in config.ini, on by default) switches it off: the
game then opens no pad and reads none of its buttons (`Core::Input::Gamepad::SetEnabled`). It is for
a pad that Steam Input or a handheld's own software (Armoury Crate, Legion Space) already turns into
a mouse and keys, so a press does not count twice. Turned off with the pad itself, the pad plays on
until a key, a mouse button or the screen is pressed (or the pad is unplugged), and the system log
says so: the player is never left with nothing to press (`Gamepad::TurnsOffWhenPutDown`).

While the pad is in use, the touch controls show in the world (in the PC view too, over the PC bar)
with the pad button that presses each one drawn on its corner: the Xbox glyphs (A green, B red,
X blue, Y yellow, the bumpers, triggers, View, Menu, LS for the left stick pressed and the d-pad's
arms), drawn from boxes in `themes/{legacy,modern}/gamepad_glyphs.rcss`, no images. In the phone view
the pad replaces the PC bar as touch does (style flag `mobile-hud`). Without touch the joystick is not drawn.

| Pad | In the world |
|---|---|
| Left stick | Walks the way it points (as the joystick) |
| Right stick | The mouse pointer; at half speed over a window, to hit small buttons |
| A | Held: attack (as the Attack button). With the pointer on a window or a control, a left click there instead |
| B | Back: closes the window on top (with none open, the system menu) |
| X, Y, RB, LT, D-pad down | The five hotkey skills: a tap makes one current, held it casts |
| LB | Pick up (held) |
| D-pad up | MU Helper on and off |
| D-pad up, down over a list that scrolls | Scroll it (the mouse wheel there, repeating while held): a shop, the commands, the quests |
| D-pad left, right | The potions Q and W |
| RT | A left click at the pointer (talk to an NPC, walk there, use a window); held, a drag |
| R3 | A right click at the pointer; on an item in the bag or worn gear it opens the item card (above), except with the storage or the chaos machine open |
| L3 | The skill picker: the stick or the d-pad move its highlight over the skills, the panel's buttons and the skill buttons on its right; A on a skill button chooses it, A on a skill puts it there; B closes. Without the controls on the screen, the game's skill list |
| View | The map |
| Menu | The menu of windows: the stick or the d-pad move its highlight, A opens, B or Menu closes; its legend shows the keys. Its Chat item opens the chat line, and the pad's keyboard with it |

On the login, server and character screens both sticks move the pointer, A or RT click, B is Esc and
Menu is Enter. The pointer starts at the screen's centre the first time the pad takes over, so A
attacks at once; later it stays where the mouse left it, so a window under it stays under it (a
lifted finger leaves it off the window: then the centre again). A question (trade, party) takes Menu
as yes after a second, and A only once A has rested a second, so A tapped in a fight goes on
attacking. A button still held when the menu closes acts only once released, so the B that closed it does
not also press Back. The first time a pad is used, the system log says how it plays; Help lists
every button on a page of its own while the pad is in use (after the touch page with touch on too),
and LB and RB turn the pages.

| Piece | File | Role |
|---|---|---|
| Device | `Core/Input/GamepadInput.{h,cpp}` | SDL gamepad events → held buttons, presses since the last frame and the sticks (radial dead zone, triggers as buttons), across up to four pads; which device the player used last. The sticks are also polled once a frame (`Poll`): SDL keeps the first value an axis reports as its rest and sends no event for it. |
| Pointer | `Core/Input/GamepadPointer.{h,cpp}` | The stick moves the SDL pointer (warped), buttons as pushed SDL mouse events with their own `which`; held buttons ORed into `IsKeyDown(VK_LBUTTON/VK_RBUTTON)`, the wheel too (`TurnWheel`, for the d-pad over a list; `Core::Input::UiScrollsAt` says whether a list there scrolls). A real mouse moving it is followed; a browser repeating a resting mouse's place (it does on layout changes) is not. |
| Mapping | `UI/HUD/GamepadControls.{h,cpp}` | Once a frame before the touch controls: presses their controls (`TouchControls::SetPadHeld`), steers (`GameLogic::Touch::Steer`), moves the menu's highlight; off the world, the plain pointer; with a text field focused, hands the pad to the keyboard. |

A DualSense plays with the same buttons (cross = A, circle = B, square = X, triangle = Y); its own
glyphs are put off (ghinzuro, 2026-10-30). No pad-only view for a Steam Deck either: its pad plays
as any other.

### The pad's keyboard

While the pad is in use and a text field has the keys (the chat line, the login's account and
password, a search or amount field), an on-screen keyboard comes up and the pad types on it; nothing
else acts until it closes. It stands at the screen's foot, at its top when the field is low (the
chat line), and shows the field's text with its cursor in a line of its own, dots for a password.
The phone's own keyboard stays down while the pad types and comes back if a finger takes over.

| Pad | On the keyboard |
|---|---|
| D-pad, left stick | Move between the keys (held, it repeats); the rows wrap round |
| A | Press the highlighted key |
| B | Close it: the chat closes as with Esc, another field lets go of the keys |
| X | Backspace (held, it repeats) |
| Y | Space |
| LT | Shift: once for one capital, twice to stay |
| RT | The symbols page, and back to the letters |
| View | English or Russian letters |
| LB, RB | Move the cursor (held, it repeats) |
| Menu | Enter: send the chat line, log in |

A key a pad button also presses carries that button's glyph. A password field gets English letters
only; a field that takes digits (`text-field--numeric`) gets a number pad. Buttons still held when
the keyboard closes act only once released, so the B that closed it does not also press Back.

| Piece | File | Role |
|---|---|---|
| Layout | `UI/HUD/PadKeyboardLayout.{h,cpp}` | The pages (English, Russian, symbols, digits), their keys and widths, and moving between keys (rows centred, up and down to the nearest key's middle). Tests: `tests/ui/test_pad_keyboard_layout.cpp`. |
| Keyboard | `UI/HUD/PadKeyboard.{h,cpp}` + `pad_keyboard.rml` + `themes/{legacy,modern}/pad_keyboard.rcss` | Shows itself for the focused field (`GetFocusedTextField`), types into it through RmlUi's own SDL path (`Core::Input::RouteToUi`: text input and key events, as a real keyboard), Enter and Esc also as game keys (`Touch::TapKey`). Depth 35 in `RmlStackingOrder.cpp`: over the windows and messages of any scene, under the loading screen. |
| Repeats | `Core/Input/GamepadStepper.{h,cpp}` | The d-pad's and stick's steps with a first pause and a repeat, shared with the menu's highlight; a held button's repeat. |
| Phone keyboard | `Core/Platform/web/WebKeyboard.{h,cpp}` | `HoldBackKeyboard` keeps the phone's keyboard down while the pad is in use. |

## How it is built

| Piece | File | Role |
|---|---|---|
| Finger router | `Core/Input/TouchInput.{h,cpp}` | SDL finger events → controls (hit-tested first) or the pointer. Virtual keys (`TapKey`, `SetKeyHeld`) that `Core::Input::IsKeyDown` ORs in, so every hotkey works unchanged. |
| Pointer gestures | `Core/Input/TouchPointer.{h,cpp}`, rules in `Core/Input/TouchGesture.{h,cpp}` | Tap, double tap, drag, long press, pinch → warped SDL mouse + pushed button and wheel events. Held buttons are ORed into `IsKeyDown(VK_LBUTTON/VK_RBUTTON)`. `Rest` takes a lifted finger's pointer off the window. `Touch::Tick` runs at the top of each main-loop pass, so a long press's right button is among that pass's events. What a landing or lifting finger does in each phase, and when a long press is due, is `TouchGesture`, tested in `tests/core/test_touch_gesture.cpp`. |
| Lost releases | `App/Platform/Windows/Winmain.cpp` (`HandlePhysicalMouseButton`, `ReleaseUnheldMouseButtons`) | A release over a window of the UI still reaches the game when the game holds that button, and after each pass's events a button the game holds that nothing holds any more is let go, so the hero never walks on. |
| World actions | `GameLogic/Touch/TouchActions.{h,cpp}` | Steer, Attack, Select/Cast/BindHotKeySlot, PickUpNearest, on `GameLogic::Automation` (the helper's and control socket's primitives). |
| Controls | `UI/HUD/TouchControls.{h,cpp}` + `touch_controls.rml` + `themes/{legacy,modern}/touch_controls.rcss` | Draws the controls, answers the router's hit test from the elements' boxes, acts once a frame (`SceneManager.cpp`, after the scene). Depth 1.06 in `RmlStackingOrder.cpp`: under every window. |
| View and controls choice | `UI/Mobile/InterfaceMode.{h,cpp}` | Reads the two Options rows, turns the touch layer on or off and sets the style flags `mobile` (phone view) and `mobile-hud` (phone view with touch controls). RCSS restyles a document for them with `@media (theme: mobile)` / `@media (theme: mobile-hud)`. |
| Hero frame | `UI/Mobile/HeroFrame.{h,cpp}`, `UI/HUD/HeroGauges.{h,cpp}`, `UI/Mobile/CompactNumber.{h,cpp}` | Fills the hero frame's part of the touch controls' model, writing only what changed (the potions are counted four times a second); HeroGauges reads HP/MP/AG/SD and EXP for it and for the PC bar alike. |
| Item card | `UI/Mobile/ItemCard.{h,cpp}`, `UI/Inventory/InventoryActionController.cpp`, `UI/Inventory/MyInventory.cpp`, `UI/Inventory/StorageInventory.cpp`, `UI/Inventory/StorageInventoryExt.cpp` | The card (a generic menu dialog); the controller opens it for a finger's long press or R3 on a bag item, the inventory for a tap, a long press or R3 on worn gear, the storages for a long press on theirs. |
| Click source | `UI/Mobile/ClickSource.{h,cpp}` | Whether a window's click came from a finger's tap, drag or long press, or from the pad's R3 (`Touch::Pointer::LastLeftPress`), so that a window can ask first only for those. |
| Finger unit | `UI/Mobile/TouchUnit.{h,cpp}`, `UI/Mobile/ButtonSize.{h,cpp}` | The unit the controls are sized in and the Button size option (Options → Interface, `OptionWindow.cpp`). |
| Neighbours | `UI/Mobile/PhoneHudLayout.{h,cpp}`, sums in `UI/Mobile/PhoneHudFit.{h,cpp}` | Moves the buffs, pets' frames, system messages, skill list, party cards and chat beside the controls; tells the world labels where the HUD lies over them; puts the notices under the open menu (`UI/Mobile/NoticesUnderPanel.{h,cpp}`, writes only on a change). Finds each document once (again every 250 ms while missing) and places a piece only when the controls, the screen or the piece's size changed, or every 250 ms. |
| Page | `UI/Mobile/PhonePage.{h,cpp}`, `Core/Platform/web/WebScreen.{h,cpp}` | Screen wake lock (let go after 5 idle minutes) and the back gesture (browser only). |

The element ids in `touch_controls.rml` are the contract with C++ (`ControlElement()`): a theme may
move and restyle a control, and where it puts it is where the control answers.

Labels are `Touch*` keys in `src/Localization/Game.*.resx`.

## Testing without a phone

Chrome's device emulation (or the built-in browser pane's mobile preset) with `?phone` on the page
reports a touch device. Synthetic touches: dispatch `PointerEvent`s with `pointerType: "touch"` and
distinct `pointerId`s on the canvas (SDL's emscripten backend turns them into finger events).
Give `pointermove` `button: -1` as browsers do: SDL reads a move with `button: 0` and a held button
as a new touch, so the finger lifts and lands again where it moved (the joystick then re-centres
under it and never walks).

A gamepad without a gamepad: in the page, replace `navigator.getGamepads` with a function returning a pad
(`mapping: "standard"`, 17 buttons with `pressed` and `value`, 4 axes, `connected: true`, an `index`
and an `id`) and dispatch a `gamepadconnected` event carrying it; SDL polls the function each frame,
so changing the buttons' and axes' values plays it. A button held when the pad connects plays as the tap
that woke it (SDL alone would lose it), and a trigger's first full pull in one frame is lost: SDL takes the
first value an axis reports as its rest. A real trigger passes through several values on the way down; a fake one should too.

On the desktop, a developer build's control socket has `pad` ([control-socket.md](control-socket.md)):
`{"cmd":"pad","action":"attach"}` plugs in SDL's virtual gamepad, `down`/`up` with a `button` and
`axis` with an `axis` and a `value` play it through SDL's own gamepad events, as a real pad.
