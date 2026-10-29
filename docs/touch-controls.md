# Touch controls (browser client on phones and tablets)

On a touch device the browser client (/play) shows on-screen controls laid out as MU's mobile games
have them, and routes fingers itself instead of letting SDL turn the first finger into the mouse,
and it switches to the phone view (below). Two rows in Options → Interface choose this, saved in
config.ini `[UI]`:

| Row | Key | Choices |
|---|---|---|
| Interface view | `InterfaceView` | Auto, PC, Phone |
| Controls | `Controls` | Auto, Mouse and keyboard, Touch |

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
| Bottom left | Joystick (floating: it appears where the thumb lands) | Walks the way it points, a few tiles ahead, re-planning as the walk runs out or the thumb turns. Letting go stops at the tile reached. |
| Above the joystick | Q, W | The item hotkeys Q and W (potions). |
| Along the top, right of the location bar | Back, Menu, Inventory, Character, Chat, Teleport, Map | Back is Esc: it closes the top window (or opens the system menu with none open). Menu opens a panel of 17 items under the row: windows, Skills (the skill list, to put a skill on a button) and View (PC / phone). The others press the window's key (I, C, Enter, M, Tab). |
| Bottom right | Attack (held) | Plain attack on the monster tapped last, else the one fought last, else the nearest within 8 tiles. |
| Around Attack | The hotkey bar's five skills | A tap makes the skill current (as a click on its slot). Held, it casts: buffs and heals on the character, area buffs around it, the rest at a monster as Attack picks one. Teleport only selects (aim it with a long press). |
| Around Attack | Putting a skill on a button | No digit keys, so either way round: a tap on an empty button (or on any while the skill list is open) lights it and opens the list, and the skill picked there goes on it; or open the list with the bar's current-skill icon, pick a skill, and the five buttons light up for 8 s for a tap on one. A hint above them says what to do. The list counts its picks (`CSkillList::GetGridPickCount`), so picking the current skill again counts too. Either way the hotkeys are saved to the server at once (`SaveOptions`), as a closed tab never logs out. |
| Right edge | Pick up (held) | Walks to the nearest drop within 6 tiles and takes it. |
| Under the skills | Auto | MU Helper on/off (the Home key); lit while it runs. |
| Anywhere else | One finger = the mouse | Tap = left click (double tap = double click), drag = left button held, hold 0.5 s = right click (on an item in the bag it opens the item card instead, below), two fingers = pinch zoom (mouse wheel). The camera's zoom lock (F10 on a computer, on by default against a stray wheel) is lifted whenever the controls show, as a phone has no F10. |

## Item card

A finger held on an item in the bag opens a card in place of the PC's right click, which would use,
equip, sell or drop the item at once: the item's name, then Use / Equip (Move with the storage, trade
or chaos machine open, Sell in a shop), Put on button Q / W (for items the hotkeys take), Drop (only
with the bag alone open, for items that may be dropped; it asks first) and Close. Each button checks
that the same item still lies there before it acts. A mouse's right click is unchanged.

## Phone view

The phone view replaces the PC's bottom bar while the touch controls show: a hero frame top left
(level, HP and MP with their numbers, SD and AG), a thin EXP line along the bottom edge, and how many
potions Q and W hold under the buttons. The PC bar (but its skill list, which the menu's Skills
opens) and the top-right buttons hide; the system messages move under the hero frame, the pets'
HP frames right of it, the buffs under the top row and the chat right of the joystick's area. Without touch controls the phone view keeps the PC bar.

The phone view also draws the login box, server list, character select buttons, system menu and
menus larger, for fingers.

Windows, the HUD bar and tooltips take a finger before the controls: a finger on an open window
plays the mouse there even where a button lies under it. A window a workspace slot placed counts by
its panel's box (`#panel.slot-placed`), as the inventory, shops, storage and trade panels take no
pointer events themselves (their item grids are clicked natively) and RmlUi's hit test looks through
them. The chat log and the scrolling notice do not take the finger (they only show text, over the
joystick's corner and the top row).

Every action keeps the MU Helper's pace (walk packets 300 ms apart, attacks and skills 250 ms):
the server reads a burst of walk packets as a speed hack.

## How it is built

| Piece | File | Role |
|---|---|---|
| Finger router | `Core/Input/TouchInput.{h,cpp}` | SDL finger events → controls (hit-tested first) or the pointer. Virtual keys (`TapKey`, `SetKeyHeld`) that `Core::Input::IsKeyDown` ORs in, so every hotkey works unchanged. |
| Pointer gestures | `Core/Input/TouchPointer.{h,cpp}` | Tap, double tap, drag, long press, pinch → warped SDL mouse + pushed button and wheel events. Held buttons are ORed into `IsKeyDown(VK_LBUTTON/VK_RBUTTON)`. |
| World actions | `GameLogic/Touch/TouchActions.{h,cpp}` | Steer, Attack, Select/Cast/BindHotKeySlot, PickUpNearest, on `GameLogic::Automation` (the helper's and control socket's primitives). |
| Controls | `UI/HUD/TouchControls.{h,cpp}` + `touch_controls.rml` + `themes/{legacy,modern}/touch_controls.rcss` | Draws the controls, answers the router's hit test from the elements' boxes, acts once a frame (`SceneManager.cpp`, after the scene). Depth 1.06 in `RmlStackingOrder.cpp`: under every window. |
| View and controls choice | `UI/Mobile/InterfaceMode.{h,cpp}` | Reads the two Options rows, turns the touch layer on or off and sets the style flags `mobile` (phone view) and `mobile-hud` (phone view with touch controls). RCSS restyles a document for them with `@media (theme: mobile)` / `@media (theme: mobile-hud)`. |
| Hero frame | `UI/Mobile/HeroFrame.{h,cpp}`, `UI/HUD/HeroGauges.{h,cpp}` | Fills the hero frame's part of the touch controls' model; HeroGauges reads HP/MP/AG/SD and EXP for it and for the PC bar alike. |
| Item card | `UI/Mobile/ItemCard.{h,cpp}`, `UI/Inventory/InventoryActionController.cpp` | The card (a generic menu dialog); the controller opens it for a finger's right click on a bag item. |

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
