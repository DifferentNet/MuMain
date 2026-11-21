# Touch controls (browser client on phones and tablets)

On a touch device the browser client (/play) shows on-screen controls laid out as MU's mobile games
have them, and routes fingers itself instead of letting SDL turn the first finger into the mouse,
and it switches to the phone view (below). Two rows in Options → Interface choose this, saved in
config.ini `[UI]`:

| Row | Key | Choices |
|---|---|---|
| Interface view | `InterfaceView` | Auto, PC, Phone |
| Controls | `Controls` | Auto, Mouse and keyboard, Touch |
| Button size (only while the on-screen controls show) | `TouchButtonSize` | 60, 70, 80, 90, 100, 115, 130 % |
| Gamepad ([below](#gamepad-xbox-layout)) | `Gamepad`, `GamepadRumble` | On, On without vibration, Off |
| Stick dead zone (while the pad is on) | `GamepadDeadZone` | 10, 15, 20, 25, 30, 35 % of the throw (25 to start with) |
| Pad pointer speed (while the pad is on) | `GamepadPointerSpeed` | 50, 75, 100, 125, 150, 200 % |
| Button glyphs (while the pad is on) | `GamepadGlyphs` | Auto (the pad in hand), Xbox, PlayStation, Nintendo |

Auto means the phone view and touch controls on a touch device (`mu::web::IsTouchDevice()`), the PC
view and the mouse elsewhere. A desktop build or a computer's browser is unchanged unless the player
picks Phone or Touch (a Windows touch screen works with Touch). The touch menu's View item switches
between the PC and phone views at once. In the browser the page's own menu (the footer on a computer)
has a View item too: it steps Auto → PC → Phone and shows the choice in force
(`Core/Platform/web/WebPageView.{h,cpp}`, `window.muView` in `src/web/index.html`; the game takes
the page's request between frames, `UI::Mobile::FollowPage`).

On a phone or tablet in the browser (`UI::Mobile::IsHandheld()`) none of this is a choice: the phone
view, the touch controls and the Legacy theme always apply (`InterfaceMode::Apply`,
`RmlTheme::ResolveStartupThemeName`; the Modern theme's windows are not ready for a phone yet,
ghinzuro 2026-11-11). A saved PC view, mouse or Modern theme stays in config.ini and applies again on a
computer. Options hides the Theme, Interface view and Controls rows there (`layout_choices_shown`), the
touch menu has no View item (`menu_view`) and the page's ⋮ menu none either.

## What the player gets

| Where | Control | What it does |
|---|---|---|
| Bottom left | Joystick (floating: it appears where the thumb lands) | Walks the way it points, a few tiles ahead, re-planning as the walk runs out or the thumb turns. Letting go cuts the planned walk short at the next tile (`Automation::StopAtNextTile`, tested in `tests/gamelogic/test_walk_stop.cpp`): the hero stops within a step instead of finishing the tiles planned ahead. The page going out of sight (`SDL_EVENT_WINDOW_HIDDEN`) lets go of every finger, as a phone sends no lift for a thumb still down when the app is switched. |
| Above the joystick | Q, W | The item hotkeys Q and W (potions). On a phone or tablet each is tinted by the potion on it (`UI/Mobile/PotionKind.h`: healing red, mana blue, shield yellow, complex violet, anything else plain); in the phone HUD the key letter is hidden (a pad alone keeps it). These and the other key buttons and menu items press the key now bound to that action (set on a computer in Options > Gameplay > Key bindings, a row phones and tablets do not show, [options-window.md](options-window.md#key-bindings), `KeyBindings::Runtime::RemapDefaultKey`), and the pad's shortcuts the same way. |
| Along the top, right of the location bar | ≡ (fold), Back, Menu, Inventory, Character, Chat, Social, Teleport, Map | ≡ folds the rest of the row away and back, saved as `[UI] TouchTopRow` (folded by default); it is gold-edged while the row shows, and folded the world labels show beside it. With the row folded the pad's Menu still opens the menu. Back is Esc: it closes the top window (or opens the system menu with none open). Menu opens a panel of up to 25 items under the row (the GM panel only for a GM, Gens only for a character in a Gens, and no View item on a phone or tablet): windows, Skills (the skill picker, below), View (PC / phone), Chat (for a pad, which has no Chat button of its own), Player actions (D, the commands for the character under the pointer), the PK switch (below), the GM panel (Y, GMs only) and Gens (the Gens window, as B opens it). The others press the window's key (I, C, Enter, M, Tab). |
| Bottom right | Attack (held) | Plain attack on the monster tapped last, else the one fought last, else the nearest within 8 tiles. Where players fight (a duel, Chaos Castle, a Gens enemy on the strife maps) a player tapped or fought counts as a monster does, and the nearest enemy player too. The target the buttons fight is lit, as a monster under the mouse is (`TouchActions::TargetIndex`, drawn in `RenderCharactersClient`); while the controls show and the MU Helper is not hunting, the monster Attack would pick is lit before the first swing too (`TouchActions::SetControlsShown`). |
| Left of Target | Loot | The drops' names on the ground on or off, as Alt and the menu's switch do (`NameWindow::ToggleItemNames`); lit while on. Hidden for a pad without touch. |
| Under the skills, left of Auto | Target | Steps to the next monster by distance (from the one lit, round the list nearest first); it becomes the one the buttons fight. Faded when no other monster is near, and a press then does nothing (`TouchActions::NextFoe`, `HasOtherFoe`). Hidden for a pad without touch. |
| Menu | PK switch | Ctrl for a finger: while on, Attack reads "PK" in red, a tap on a player attacks and the buttons fight the player tapped last, as Ctrl and a click do on a PC (`TouchActions::SetForceAttack`, the engine's `CanAttackCharacter`). Never a member of the party; off again on a new map or at death. |
| Party cards | A tap on a member's card | The skill buttons' heals and buffs go to that member, walking up to them while they are in sight (14 tiles), and the card stays lit. Out of sight (farther, another map, no way to them) the chat says "Too far away" with their name and nothing is cast; a member who leaves the party is forgotten. A tap on the card again (or on the character's own) gives the heals back to the character (`TouchActions::ToggleFriend`). The card's leave button is no pick. |
| Around Attack | The hotkey bar's five skills | A tap makes the skill current (as a click on its slot). Held, it casts: buffs and heals on the character, area buffs around it, the rest at a monster as Attack picks one. Teleport only selects (aim it with a long press). A skill the character lacks the mana or AG for shows a blue veil and a faded icon on its button (`TouchActions::HotKeyShortOfMana`); the chat says it at most every 6 s, the potion request still goes every 1.5 s. A skill that cannot go off yet (out of reach while the stick walks, refused by the engine) gives its turn back, so a held Attack keeps swinging. Held buttons act newest press first. Nova charges while its button is held and goes off when let go (or by itself after 5 s), as the PC's right button does: the press sends the charge start, the release the blast (`TouchActions::ChargingHotKey`, `ReleaseCharge`); a quick tap fires a weak one. A buff still on the one it goes to (the character, or the party member chosen; `TouchActions::HotKeyBuffRuns`, the buffs from `GameLogic/Skills/SkillBuffs`, MU Helper's table) puts a thin green ring inside its button, and held the button does not cast it again (a tap still does); pad and touch alike (pad round 6). A mage's area skills aim for the pack (pad round 6, pad and touch only, `TouchActions::CastAtMonsterAimed`): Flame, Cometfall, Decay and Ice Storm go on the monster within reach that has the most others in its area, counted on whole tiles as the server counts the hits (the 3 x 3 for Flame, Cometfall and Ice Storm, Decay's 5 x 5 square; `GameLogic/Touch/AreaAim::PackCentre`, ties to the one nearest the target), Twister and Aqua Beam fly the way the joystick or the left stick pushes, at the farthest free tile within reach (`AreaAim::AlongLine`); at rest, on the target as before. Only while the target itself is within the skill's reach with no wall between: otherwise the cast goes at it and walks up, as before. The target stays the aim of every skill when it was chosen with the gold ring, aimed by hand (the pad's pointer or a tap on it) or is a player; only monsters make a pack. Mouse play is unchanged. |
| Around Attack | Putting a skill on a button | The skill picker: a panel on the left of the dimmed screen lists the skills (icon and name, as many to a page as fit, < > turn the pages), the five skill buttons stay lit right of it. The button being set glows gold; a tap on another button chooses that one. A tap on a skill puts it there and the next empty button is chosen, so a new character fills the five with five taps. A skill learned in play (an orb, a scroll, a skill weapon) while the controls show goes on the first empty button by itself, never over a filled one, and its "New skill" line names the button (`OfferNewSkill`; the skills a login lists are left as they are). A skill already on another button swaps places with the one it replaces (each skill is on one button at most; a gold dot marks the skills on a button, the chosen button's skill is lit). Remove empties the chosen button, Done, Back or a tap beside the panel close it. It opens from the Skills button beside the arc, from Menu → Skills, or from a tap on an empty skill button (which shows a plus). The hotkeys are saved to the server at once (`SaveOptions`), as a closed tab never logs out. The rules (which hotkey a button is, swapping, the next empty button, how many skills a page holds) are `UI/Mobile/SkillArrange.{h,cpp}`, tested in `tests/ui/test_skill_arrange.cpp`. The game's own skill list (the PC bar's current-skill icon) still binds as before: with it open, a tap on a button and then a skill, or a skill and then a button within 8 s. |
| Left of the skill buttons | Skills | Opens the skill picker on the first empty button (else the first). |
| Over Pick up | The quick equip offer (pad round 6) | A picked-up item the hero may wear and that raises their Combat Power (the item tooltip's CP) shows "New: <item> +N CP" for 10 s: a chip over Pick up for a finger (a tap puts it on), a box over the pad's row with "Hold to put on" for a pad (d-pad right held). Nothing goes on by itself: the offer goes when its time runs out, the item leaves the bag, the map changes or the hero dies, and a better pick-up replaces it. Putting on is the X swap above. Mouse play shows none (`UI/Inventory/QuickEquip`, rules in `GameLogic/Items/EquipOffer`, tested in `tests/gamelogic/test_equip_offer.cpp`). |
| Right edge | Pick up (held) | Walks to the nearest drop within 6 tiles and takes it. A drop the server refuses twice (another's loot, no room) is passed over for 30 s; a drop that is gone or replaced on its tile is not. |
| Under the skills | Auto | MU Helper on/off (the Home key); lit while it runs. While the stick walks, or a button is held or owed (and 1 s after), the helper only drinks and buffs (`CMuHelper::SetSteered`), so it does not fight the buttons for the target. |
| Anywhere else | One finger = the mouse | Tap = left click (double tap = double click), drag = left button held from where the finger landed, hold 0.5 s over a window, 0.8 s over the world = right click (`Gesture::WorldLongPressTime`: a thumb resting on the world is not a skill cast; on an item in the bag or worn gear it opens the item card instead, below; not while an item hangs on the pointer, so it can still be dragged), two fingers = pinch zoom (mouse wheel; a second finger that lands while the first drags calls the drag off, and a finger landing again while one of the pinch's is still down pinches on). A tap on the game ends typing (the chat lets go of the keys), as does any key button but Enter and Esc. A drag the browser takes away, or a turn of the screen (a resize by more than a fifth of a side, `UI/Mobile/BigResize`, checked in `HandleWindowResize`; the browser's bar sliding in or out keeps held fingers), drops and clicks nothing, and an action tapped before it (or before the app went to the background, or while the stick walked) is not carried out afterwards (`Touch::Resets`). The camera's zoom lock (F10 on a computer, on by default against a stray wheel) is lifted whenever the controls show, as a phone has no F10. |

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
`generic_menu_dialog.rcss`). In the phone view its buttons are 42 units tall (native's 29) and 3 apart; a
long list (a potion's seven buttons and Close) scrolls inside the card, at most 300 units of it, so the card
stays on a 400-pixel-tall screen.

A finger held on a "+" of the character window (a right click, R3 on a pad) opens a menu of bigger
steps for the free points: +10, +100, +1000, +5000, +10000 while there are more, then all of them, spent through
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

**Junk (pad round 6, pad and touch only).** The card of a plain weapon, shield or armour in the bag
has Junk (Not junk once marked); the marked item shows a gold coin on its corner. The bag's coin button
left of Sort marks every plain item at once ("Mark plain gear as junk") and says how many. Only plain
gear can be junk: never excellent, ancient, +7 or higher, with luck, sockets or a harmony option, nor the chaos
weapons the wing mixes take, rings,
pendants, wings, jewels, quest items or items a shop does not buy. A mark keeps its item's fingerprint
(type, level, options), so it follows the item through the player's own sort and never passes to another item
in the cell (any other inventory list only drops marks whose cell now holds another item); the marks are kept per character in `config.ini` `[Junk]`. In an NPC shop "Sell junk (N)" (a coin
and the count, left of the repair buttons) asks once with the count, every item's name (the list scrolls) and the total Zen,
then sells the items one at a time, each after the server's answer to the one before; each is checked
against the rule again right before it goes. It stops at a refusal, at the Zen limit, when the shop closes, on a warp or
death, or after 5 s without an answer, and says how many were sold (`UI/Inventory/JunkSale`, rules in
`GameLogic/Items/JunkMarks` and `JunkMarkSet`, tested in `tests/gamelogic/test_junk_marks.cpp`).

In the storage and the extended storage a finger held on an item opens a card with Move to bag (a
right click moves it at once). The chaos machine's long press still moves the item back at once.
With the storage, a trade or the chaos machine open, a finger's double tap on an item moves it there
from the bag or back to the bag, as the right click does (round 16; never in a shop or with the bag
alone; the first tap's lift is put back first, `CInventoryActionController::MoveOnDoubleTap`). A line
says so the first time one of them opens under fingers in a session.

In an NPC shop a tap selects an item: its cells light up, its tooltip shows and, once a shop opening,
a line says to tap it again; a second tap on the same item within 1.5 s buys it. A potion or another consumable the potion keys take
stays selected after a buy, so each further tap within 1.5 s of the last buy buys one more (10 potions: 11
taps); gear is selected again after each buy. A tap on another
item selects that one, and a drag lifted on the grid buys nothing. The mouse and the pad buy with one
click, as before; a gamble shop asks first anyway (`UI/NPCs/ShopTapToBuy.{h,cpp}`, tested in
`tests/ui/test_shop_tap_to_buy.cpp`).

## Phone view

The phone view replaces the PC's bottom bar while the touch controls show: a hero frame top left
(compact bars on a dark plate, the level's number in a circle with a master level's "+N" under
it, HP and MP 16 units tall with their numbers, SD and AG 5-unit bars without), a thin EXP line along
the bottom edge, and how many
potions Q and W hold under the buttons. The PC bar and the top-right buttons hide (the skill picker
stands in for its skill list); the system messages move under the hero frame, the pets'
HP frames right of it, the buffs under the top row and the chat right of the joystick's area. Without touch controls the phone view keeps the PC bar.

With the touch controls (the phone HUD) the hero frame also:

- warns of low HP: under a quarter of the hero's HP, the HP bar gets a red border (#ff5040) and its fill
  pulses (0.8 s); a dead hero (no HP) does not pulse;
- opens the character window when tapped (as C does; the frame lights while pressed). It is hidden over
  the full-screen map, and a hidden frame takes no finger; the pad has no glyph or highlight for it;
- shows the EXP line's percentage with one decimal ("42.7%") as its last row, centred under SD and AG
  (the bottom centre over the EXP line is the chat's); the frame grows by that row and the system
  messages under it follow. It is cut, not rounded, so it reads 100 % only when full, and it hides over
  the map with the frame;
- shows a gold "+" on the level's circle while there are stat points to spend.

A gold dot on the top row's fold button (≡) and on the menu's Friends item says an unread letter or a new
chat-room line waits, as the PC bar's Friends button does (without the blinking).

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
lies there. So does a pointer a window opens under once the finger is up (Storage after the tap on
Baz, an NPC's shop after the tap on the NPC): only a window that was not
shown the frame before counts (one that takes another's place in the same frame too, Storage as the
NPC's menu closes), so the pointer stays over a window already open, and a window that
opens under a finger still on the screen (a long press's item card) keeps it too. A pad's pointer is
the player's own and stays.
The game draws no pointer sprite for a finger (`RenderCursor`, `Touch::Pointer::FingerHasPointer`):
none from the start, and none after a tap. A mouse button or the pad pressed since brings it back,
and the pad's pointer shows whenever it plays one.

Every action keeps the MU Helper's pace (walk packets 300 ms apart, attacks and skills 250 ms):
the server reads a burst of walk packets as a speed hack.

## Sizes: finger units

The controls are sized for the finger, not for the game's resolution: `touch_controls.rcss` (both
themes) and the pad glyphs it shows (`gamepad_glyphs.rcss`) write their sizes in `rem`, and C++ sets the
document's font size to one *finger unit* (`UI/Mobile/TouchUnit.{h,cpp}`). In the browser a unit is
one CSS pixel on a phone (a 44-unit skill button is 44 CSS px, Google's and Apple's smallest touch
target being 44–48), up to 1.15 on a tablet (the short side over 412 CSS px), times the player's Button
size, but never more than the screen's height holds (the left column of top row, hero frame and
potions over the joystick is 320 units tall): `UI/Mobile/ButtonSize.{h,cpp}`, tested in
`tests/ui/test_button_size.cpp`. It follows the canvas's size on the page, measured again when the window
or the percent changes and every second (the browser's zoom changes it alone). Off the web a unit is
RmlUi's dp at the player's percent. The finger's slop, the stick's reach and its dead zone are in units
too.

Only what lines up with the PC's own HUD stays in dp: the top row starts right of the location bar
(214dp), and the hero frame sits under it (4dp, 26dp, 170 units wide).

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
buttons'. The notices lie under every window, the open menu and skill picker included
(`RmlStackingOrder.cpp`; `UI/Mobile/NoticesUnderPanel` never raises them). While a window covers part
of the screen (the inventory, the character), the notices centre in the world it leaves, kept whole on
the screen; the PC centres them the same way (`UI/HUD/Notices.cpp`). The notice ticker runs in a strip under the top row, right of the hero frame, clipped to it
(`slide_notice.rml` `#lane`). The PC's HUD shows all of these as before.

Wherever the touch or pad controls show, the skill buttons and Skills hide until the character knows a
skill (a new character has nothing to put on them). Also in the phone's HUD: the MU Helper bar keeps only the position readout (its
config, start and stop buttons are 18x13 dp, and Auto and Menu -> MU Helper do the same); the chat's
input bar is drawn at at least 1.4 finger units per unit (its slot's `data-touch-scale`,
`docs/rmlui-ui-system/window-placement.md`), so its type buttons and fields take a finger.

In the phone's HUD the top row's buttons are as wide as their labels, 44 to 72 units (a wide phone
looks as before), and the labels get smaller, not under 9 units, where the row is too short for them
at 12 units: Russian's on a phone 640 CSS px wide. The buttons on the right are 15 % smaller than on a
PC touch screen (ghinzuro 2026-11-11): Attack 72 units, the skills 44 on an arc 124 units from
Attack's centre, the round buttons (Auto) 38, Pick up and Loot pills 54 wide in the Modern theme
("Поднять" needs 48 at 12) and 60 in the Legacy one (DejaVu Sans, 54), Target 60 ("ターゲット"), Skills 54 x 28. The skill picker's Remove, Done and page buttons are 44
units tall; no text is under 12 units. Below 100 % Button size the labels keep their size on the
screen: the controls' body gets the class `size-60` .. `size-90` (`size_pct`), and the labels' font is
12 x 100 / percent units, with Pick up, Target, Loot and Skills wider to hold it (Loot moves left with
Target; Auto's label stops growing at 15 units, as much as its round button holds). The top row's
labels and widths and the whole hero frame grow the same way, so they look as at 100 %. The hero frame's
HP and MP show their numbers in full. In the phone's HUD (`mobile-hud`) the panels (the menu with its
items and legends, the skill picker, the Social panel with its cards and actions), the potions' counts
and the bind hint's text grow the same way (the generated `body.size-XX` block inside `@media (theme:
mobile-hud)` at the end of `touch_controls.rcss`, both themes; the bind hint keeps its place, only its
text, padding and corners grow; the pad glyphs inside the legends keep their size), and the menu's,
the picker's and the Social panel's finger slop and the picker's page gap follow (`PanelUnitPixels` in
`TouchControls.cpp`, only while `ReplacesPcHud()`; used by `MenuHitTest`, `PickerHitTest`,
`SocialHitTest` and `MeasurePickerPage`). A PC with a touch screen or a pad keeps the controls' own
sizes, and so does the pad's row along the bottom.

In the phone's HUD a unit is also never wider than the screen's width holds for the top row: the row
needs 214dp (the location bar's corner) plus 332 + 21.4 x 9 = 524.6 units at its smallest labels with
fingers, 278 + 30.9 x 9 = 556.1 with the pad's glyphs (`ButtonSize::TouchTopRowUnits`,
`PadTopRowUnits`), so a unit is at most (width - 214dp) / those units (`ButtonSize::UnitCssPixels`, its
fourth argument the row's units, 0 outside the phone's HUD, the fifth the dp in CSS px; above 100 % the
cap stays the 100 % one, below it shrinks with the percent). A phone 667 x 375 gets a smaller unit,
915 x 412 and 874 x 402 keep 1 (750 x 381 too with fingers, not with the pad). Desktop /play and a PC's
touch screen are not capped.

The buttons light while held, and a tap shorter than 120 ms still lights its button that long (the
`pressed` class, `SyncPressed` in `TouchControls.cpp`): Attack (its own red), the skills, Pick up,
Auto, Target, Menu, Social, the key buttons (potions, Loot, the top row) and Skills; fingers and the pad
both light them. On a PC the pad row lights the same way while its pad button is held (X held shows
before the skill animates).

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
  and steps forward onto it again. A press closes the touch menu if it is open, else does what Back does
  (`TouchControls::PressBack`); out of the world it is Esc. The entry is added with a tap or a key behind it
  (the tap on Play, else the next one), and not again after each press: Chrome's Back skips the entries a
  page adds with no tap or key since its last step through its history (its history manipulation
  intervention), so a second Back in a row left the game, and closed the installed app (round 33). Once the
  game has stopped (Exit, a crash), Back is the browser's again: the first steps off the game's entry, the
  next leaves the page.
- Frame pace (`WaitForBrowserFrame` in `Winmain.cpp`): the world runs at the FPS cap of the options
  (60 when none), the menus at 30 at most, loading with no cap; a hidden page runs at 25, its wait timed by a
  small worker (`Module.muHiddenClock`, setTimeout if there are no workers), as browsers slow a
  background tab's own timers to once a second (MU Helper stood still); a phone freezes the whole
  background page anyway. A phone whose frames
  cost more than 24 ms on average drops to 30 until they cost under 16 ms again. Frames wait on the
  browser's `requestAnimationFrame`, not a timer, so they land on the display's refresh. The web build
  starts no animation thread pool and no CPU-usage thread: a phone's few cores go to the browser.
  The touch labels refresh 4 times a second, not every frame.
- Window warm-up (`UI/Core/WindowWarmUp.{h,cpp}`, the whole browser build, PC and phone): 5 s after
  entering the world or another map, while frames take under 40 ms (80 ms once they have been heavier
  for 30 s) and no mouse button or finger is down, windows are each shown unseen for one RmlUi frame
  (opacity 0, no pointer, never open for the game), so their first layout and images are done before the
  player opens them. First the hotkey windows (and the phone's top row and menu), then a second list: the
  system menu, the NPC windows (warmed with an empty model, nothing is sent to the server) and the windows
  behind a button. The order and what is left out are at `kWarmUpWindows` in `UI/Core/WindowWarmUp.cpp`.
  Once a session each; one with nothing to show (open already, no map picture, not for this class or
  device) is tried again in the next map. config.ini `[UI] WarmUpWindows` (1 by default, 0 turns it
  off). The `render` log says `Warm-up: <window> in <ms> ms (frame <ms> ms)` and `Warm-up: done, N windows`.

## Windows in the phone view

Each window restyles itself under `@media (theme: mobile)` in both themes; on a PC nothing changes.

| Windows | What the phone view does |
|---|---|
| Inventory, storage, shops, trade, chaos machine, personal shops | The inventory alone fills the screen (equipment left, bag right, cells about 40 CSS px). Beside another window `#panel` gets the class `beside`: the bag only. The 15-row grids show part of the grid in an `.item-grid-viewport` paged by ▲▼ (`data-grid-scroll`), the arrow that cannot move further dimmed (`at-end`); a drop outside the shown rows is refused. The 4-row windows put the grid right, text and big buttons left. |
| Character, quests, party, friends, battle pass, help, options, teleport | Wider panels (`docked_panel_frame.rcss` repeats the frame's middle so the art stays undistorted), bigger tabs, rows and buttons, a corner close a finger hits (56 × 50 px). |
| MU Helper | The settings 640 units wide in two columns, the pieces a third larger, a check box's finger area running on over its label, and a line saying how to clear a skill slot (hold it; R3 on a pad). Its Setting panel beside it, 300 units wide: each pane the panel's width, the condition radios in one column, long labels on two lines, a whole check row a target, the gauges half as large again, Save and Initialization the panel's width. The skill picker's entries 40 × 46 in columns of eight. The pieces both windows share are in `mu_helper_common.rcss`'s phone block. |
| Commands | A side panel 300 units wide (`chat_command.rcss`): rows 34 units tall, every command listed and scrolled by finger instead of by page; a command's page fits its description, each parameter's name over a 30-unit box, and the actions to the panel (`CChatCommandWindow::CurrentPage`, re-wrapped when the view changes). |
| Skill list (the PC bar's current-skill icon) | Rows of eight finger-sized cells on a dark plate above the controls. Back closes it (Esc would open the system menu over it). |
| Map (Tab) | Full screen, under the touch controls so their Back closes it. While it is open the controls' body gets `over-map` (`over_map`, with fingers only): the hero frame, EXP and its percentage, potions, Attack, the skills, Skills, Pick up, Auto, Names, Target and the bind hint hide, so only the top row and the joystick stay over the map. |
| Confirm dialogs (`generic_confirm_dialog.rcss`, legacy), event results (`message_box_view.rcss`, legacy) | Buttons 84 x 44 (three: 66 wide), 16 above the bottom edge and further apart, 66 under the text for them; the Blood/Chaos Castle and Devil Square results' OK 84 x 42. The Modern theme lays these out its own way and is unchanged. |
| Corner close (`base.rcss`, both themes) | 56 x 50 px. In the inventory (legacy) the bag tabs (60 x 40) and Sort (as tall) move 40 px left to clear it, and the gold box is 40 px narrower. |
| Character (legacy) | The stat "+" buttons take a 50 x 50 finger area round their art (padding with a negative margin). |
| Party, quests, jewel bank | The party's kick cross takes a 41 x 41 finger area round it; the quest progress window's page arrows sit further apart (the left one at 112 px) with 8 px around each, and the answer rows get 6 px above and below in the quest progress window and 4 px in the NPC quest window (its dialogue band is centred on 7 lines, so a full band with 4 answers still clears the title and the divider). The jewel bank's zen buttons are narrow and as tall as the row (30 x 31: 5 px above and below their art, 2 px beside it), moved left (106 and 138 px); the name and the count are cut 74 px wide so they stop short of them. The art stays where it was (padding with a negative margin, the face painted on the content box); in the Modern theme the kick cross and the zen buttons lose their thin border on the phone. |
| Social | Fewer players are listed while the panel would run past its foot (a short screen, many actions): the last row of cards goes until the panel fits, never the first row (`FitSocialPlayers`, measured on the frame's layout; the limit resets when it opens). A player chosen by a tap who is still around but past the cards that fit takes the last card; one who leaves leaves nobody chosen, and the actions wait for a new tap. Party says `TouchMenuParty`. |
| Chat input (legacy) | Quick buttons over the bar in one line the bar's width, dragged sideways for the rest: the saved macros of keys 1 to 5 (only those set; a tap sends one as Alt+N does), then the twelve quick chat phrases (pad round 6, see the pad's keyboard below: each reader sees them in their own language, 2 s pause shared with the macros). A tap sends one as Enter sends a line (`CChatInputBox::SendPhrase`); a line half typed comes back in the bar after it. Hidden off the phone view and in the Modern theme. |
| Touch menu | Drawn over every window while open (`OpenTouchMenuDepth`); a tap on the dimmed screen around it shuts it, and it shuts while a dialog waits for an answer. |
| Map (Tab): names | A tap names the marker under it, or else the nearest drawn one within 22 finger units at 100 % (`TouchUnit::FullPixels`), live event monsters first (`UI::MiniMap::PickHint`, tested in `tests/ui/test_mini_map_layout.cpp`). A mouse names only the marker under it, as before. |
| Teleport (warp list) | Rows 44 units tall (were 30); a pressed reachable row lights up as a hovered one does (`move_command.rcss`). |
| Party, no party yet | With the phone HUD (`ReplacesPcHud`) the hint leaves out the mouse lines ("Type /party with the mouse cursor on…") and keeps why to party; under it a button 48 units tall, labelled `TouchSocialTitle`, closes the window and opens the Social panel, where a party is offered with a tap (`TouchControls::ShowSocialPanel`). Hidden in a party and off the phone HUD. |
| Jewel bank: names | When a name is too long for the narrow name column, the words most names start with go ("Jewel of", "Камень"): words split on spaces only, shared by more than half of the shown rows and at least two (`UI::Inventory::JewelBankNames`, tested in `tests/ui/test_jewel_bank_names.cpp`). |
| Event timer (O) | 270 units wide (was 190) with the character window's phone frame pieces; the name and time lines 226 wide, so the start times are not cut (`CEventScheduleWindow`'s `PHONE_CONTENT_WIDTH`). |
| Character creation | The class buttons 34 units tall (32 in the Modern theme) and spaced to fill the class panel; OK and Cancel 38 units tall (36) beside the name row (`char_make.rcss`). The class description below keeps its place. |
| Guild, no guild yet | With the phone HUD (`ReplacesPcHud`) the three hint lines become one wrapped line (`hint_text`, built by `JoinHintLines` in `GuildInfoWindow.cpp`), and a button 48 units tall labelled `TouchSocialTitle` closes the window and opens the Social panel, where a guild is offered with a tap (`TouchControls::ShowSocialPanel`). Hidden in a guild and off the phone HUD. |
| Blood Castle / Devil Square entry | The level buttons at least 42 units tall with 6 between them (`event_entry.rcss`); when the window opens, the list scrolls so the button the hero may enter is in view (`EventEntryView::ScrollEnabledButtonIntoView`). |
| Options | The first time it opens in the phone view with touch controls it shows the Interface tab (button size, view and controls rows); a tab the player picks afterwards is kept (`COptionWindow::SelectFirstPhoneTab`). |

A finger that moves on a scrollable list scrolls it, as on a phone, rows and buttons included (they
click only on a tap); a flick glides on and a tap on a gliding list only stops it. Item grids,
scrollbars, sliders (RmlUi's `drag` property other than `block`) and text fields keep the drag;
a slider inside a list that scrolls up and down (Options) takes the finger only when it starts
sideways, and an up or down swipe over it scrolls the list (`TouchGesture::IsMostlyVertical`)
(`Render/RmlUi/RmlTouchScroll.{h,cpp}`, reached from `Touch::Pointer` through
`Core::Input::BeginUiTouchScroll` and the other `IUiInputConsumer` touch-scroll calls).

The first time fingers open the bag in a session (no pad in use), the system messages say once what
holding does (`TouchHelpItem`, `HintHoldOnce` in `TouchControls.cpp`).

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

The pad shakes (unless "On without vibration" is chosen): a hard blow as hard as it hurt, the death
long, a level up with one soft swell, a drop the loot filter highlights within 8 tiles with a light
tick (sound on or off), and a target picked with the right stick with a 40 ms light tick. A weaker
shake that would end while a stronger one still runs is dropped, so a pick's tick does not cut a
blow's shake short; a weaker one that runs on past it (a level up after a blow) plays. A shake the pad
refused (no motors) does not count as under way (`Core/Input/GamepadRumble.{h,cpp}`, tested in `tests/core/test_gamepad_rumble.cpp`).
Life falling to a quarter or less (life alone, the shield not counted) gives two short pulses 0.18 s
apart (pad round 7), not in a safe zone and not when dead; when the blow that took it there shakes the
pad, the first pulse waits until that shake ended and 80 ms more, so the two do not run together; again only once life went back over 40 % and
10 s passed (`Core/Input/LowLifePulse.{h,cpp}`, tested in `tests/core/test_low_life_pulse.cpp`).

On a PC (the browser's Gamepad API tells no battery) a pad on its own battery at 20 % or less says
"Gamepad battery low: N%" once in the system messages, and again at 10 % (pad round 7); on the cable,
or back over 25 %, it may say it again, and a line not said yet is dropped. The line waits until the hero is in the world
(`Core/Input/GamepadBattery.{h,cpp}`, tested in `tests/core/test_gamepad_battery.cpp`; the control
socket's `pad` `battery` action fakes the battery).

While the pad is in use, the touch controls show in the world (in the PC view too, over the PC bar)
with the pad button that presses each one drawn on its corner: the Xbox glyphs (A green, B red,
X blue, Y yellow, the bumpers, triggers, View, Menu, LS for the left stick pressed and the d-pad's
arms), drawn from boxes in `themes/{legacy,modern}/gamepad_glyphs.rcss`, no images. Button glyphs
in Options picks another maker's names: PlayStation draws the cross, circle, square and triangle and
names the shoulders L1 R1 L2 R2; Nintendo shows white letters, B A Y X by place, and L R ZL ZR.
The pad is read by place either way, so the bottom button confirms on all three (ghinzuro
2026-11-13: Nintendo letters only, confirm stays on the bottom). Auto takes the kind of the pad
pressed last: SDL's type on a PC, else the pad's name (the browser's Gamepad.id with Sony's 054c or
Nintendo's 057e vendor id, `Core::Input::Gamepad::KindFromName`); the names are `UI/HUD/PadGlyphs`'s.
The texts name the buttons the same way: Help's pad page, its LB/RB hint, the skill picker's tips and
the chat's lines say `{A}`, `{B}`, `{X}`, `{Y}`, `{LB}`, `{RB}`, `{LT}`, `{RT}`, `{LS}`, `{RS}`, `{MENU}`
and `{VIEW}` in all 11 languages, and `PadGlyphs::TextNames` puts in the pad in hand's names (Xbox
letters; PlayStation Cross, Circle, Square, Triangle in the player's language, L1 R1 L2 R2 L3 R3,
Options and Create; Nintendo B A Y X by place, L R ZL ZR, + and -), through the pure
`UI/HUD/PadButtonNames` (tests: `tests/ui/test_pad_button_names.cpp`).
In the phone view
the pad replaces the PC bar as touch does (style flag `mobile-hud`). Without touch the joystick is not drawn.

In the world the pad plays as Diablo's pads do (layout A of 2026-10-31): no pointer (none is drawn), a row along the
bottom: the potions LB and LT, attack, the three skills X, Y and B (RT held shows 4 and 5 in their places, RB held 7 to 9) and MU Helper. The quick row, the pickup button and the
skill edit button are not drawn for a pad without touch (`body.pad.no-touch`).

| Pad | In the world (no window open) |
|---|---|
| Left stick | Walks the way it points (as the joystick). Its walk waits until the buttons acted that frame, as the joystick's: with A or a skill held beside a monster the hero fights on, A let go and the stick walks away. Into a wall, the same way is tried again every 0.3 s, a turn of the stick at once |
| Right stick | A flick picks the next target: right or down the next further, left or up the next nearer. With no monster (or enemy player) within 8 tiles, as in a town, it picks the NPCs and the other players on the screen the same way: the one chosen is lit, its name shown as under the mouse, with "A Talk" (an NPC) or "A Menu" (a player) under its feet |
| RS (the right stick pressed) | Back to the nearest target; in a town, no NPC or player chosen |
| A | Attack: a tap fights the target once, held it goes on. On an NPC chosen: walks there and talks (as a click on it). On a player chosen: walks up to them and opens the player menu (Trade, Buy, Party, Follow, Duel, as Alt and a right click open it): the frame rests on its rows, the d-pad moves it, A runs the row, B closes |
| X, Y, B | The skill buttons 1–3: a tap uses the skill once, held it goes on. With fingers off the screen only these three show in the row (ghinzuro 2026-11-03: two or three skills carry the game). Teleport (ghinzuro 2026-11-04) jumps the way the left stick points (centred: the way the character last walked, screen-up before any walk) onto the farthest free tile within the skill's reach, at least 2 tiles (`TouchActions::PadTeleportTile`, cast through `Automation::CastSkillAtTile` with `Combat::SetGroundAim`); one jump a press. Teleport Ally brings the party member chosen with RB, or says to choose one first |
| RT held + X, Y, B | The skill buttons 4, 5 and 6. While RT is held (no window open) they stand in the places of X, Y and B (`TouchControls::SetPadAlt`, body class `pad-alt`). The sixth (hotkey 6) is a pad's alone: it shows only with no finger on the screen and the lower hotkey set (`TouchControls::ActiveSlots`, `UI::Mobile::SlotCount`); touch keeps five |
| RB held + X, Y, B | The skill buttons 7, 8 and 9 (hotkeys 7 to 9, pad round 6): a pad's alone, as the sixth (no finger on the screen, the lower hotkey set; `UI::Mobile::SlotCount` is 9). While RB is held (no window open) they stand in the places of X, Y and B (`TouchControls::SetPadThird`, body class `pad-third`); RB wins over RT's page for the skills, the potions still follow RT. A press keeps the page it began on (`GamepadPageLatch`), so letting RB go mid-hold does not switch a held skill. As soon as a pad alone has a skill on 7 to 9 (set in the picker or carried over), the system messages say how to cast it, once ever (`PadHints::Hint::SkillsThird`, remembered in `[UI] GamepadHintsSeen`) |
| LB, LT | The potions Q and W (E and R are on the keyboard only: no layout shows their buttons now) |
| RB (a tap) | The party member heals and buffs go on (ghinzuro 2026-11-04, `TouchActions::ChooseNextFriend`): since pad round 6 on the release of a tap (under 0.4 s, no X, Y or B pressed meanwhile; `GamepadControls` `RbTapTime`), each tap the next member near, their card lit and their name in the chat; past the last, back to the character itself. With no member near, it says so. A heal, a buff or Teleport Ally on the chosen member walks up to them while they are in sight (`FriendSightRange`, 14 tiles); out of sight the chat says "Too far away: <name>" (`FriendTooFar`) and nothing is cast |
| D-pad left | A tap: the map (Tab). Held half a second: the Teleport window (M). With the map up, B closes it and casts nothing, and the windows the d-pad and Menu open close it first (pad round 7: the map keeps every key but Tab to itself); its close hint names the pad's B while the pad is in use and then stands beside the cross too (its tooltip needs a pointer) |
| D-pad right | The inventory (I). With the quick equip offer up (What the player gets, above), a tap opens the inventory and held half a second it puts the offered item on |
| D-pad up | A tap: MU Helper on and off. Held half a second: its settings (Z) |
| D-pad down | Pick up: held, it gathers the drops around; with none near, a press talks to the nearest NPC. The drop it would take first shows its name and, under it, the d-pad glyph with "Pick up" |
| L3 | The skill picker: the stick or the d-pad move its highlight over the skills, the panel's buttons and the skill buttons on its right; A on a skill button chooses it, A on a skill puts it there; B closes. Without the controls on the screen, the game's skill list |
| View | Social: trade, party, guild with the players around |
| Menu | A tap: the hero hub (below). In the hub, or over another window, the menu of windows (the stick or the d-pad move its highlight, A opens, B or Menu closes; its legend shows the keys). Held half a second: the chat line, with the pad's keyboard |

**The arrow keys** (pad round 7, `UI/HUD/ArrowWalk`, the way in `Core/Input/ArrowSteer`, tested in
`tests/core/test_arrow_steer.cpp`) walk the hero on a keyboard as the left stick does: up on the
screen is up, two arrows walk the slant. Only in the world with nothing else wanting the keys: no text
typed in, no game window or dialog open, no control holding the keys, the pad not in use, no other
device walking the hero, the hero alive and no Ctrl, Alt or Shift held. A walk cut short by one of
these waits until the arrows are let go, and arrows pressed for something else (the chat's caret, a
list) walk only once let go and pressed again after it closed. Debug builds keep the arrows for the editor's hero move, the
editor's free camera keeps them too. Help's key page has its row.

**The hero hub.** Menu with no window open opens the character's windows as tabs: Inventory,
Character, Skills (the skill picker), Quests, Guild, Settings, Controls. The strip of tabs stands at the top
with LB and RB on its ends, and LB and RB turn the tabs (round the ends); the hub opens on the tab it
was last on. Closing its window (B) ends it; Menu in it goes to the full menu of windows.

**The controls card** (pad round 3, `UI/HUD/PadCard`, drawn in `pad_overlay.rml`): every button and
what it does, a page for the world and one for the windows, with the glyphs of the pad in hand. It
shows once by itself, at the first press of the pad in a safe zone with no window open, so never over
a fight (`[UI] GamepadCardSeen`; it replaced the long line the system log said), and opens from the
hub's Controls tab. With touch on (five skill buttons) its row says "Skills 4 and 5" and draws RT with
X and Y alone; without touch, "Skills 4–6" with X, Y and B, and a row "Skills 7–9" with RB and X, Y and B (`TouchControls::SkillButtonsInPlay`, as
Help's pad page and the picker's tips). LB and RB or the d-pad's sides turn its page, B, A or Menu closes it; opened from
the hub, LB and RB turn the hub's tabs and the d-pad's sides its page. A key, a click or a finger
closes it.

**Windows without a mouse.** Over a game window (not a dialog alone) no pointer is drawn: a gold frame
rests on an item cell, an equipment slot or a button (any element that binds a click, a field or a
list too; not the cross in a window's corner, as B closes the window), and the d-pad and the left stick hop it to the next one that way (in line before aside,
the same window before the next window); the character stands meanwhile. The stick hops once leaned 60 % of
the way and stops below 40 %, and keeps its way until the other axis leans 1.3 times more, so a stick held near a
diagonal hops once, not on its wobble; held 1.2 s, the stick hops quicker (every 60 ms, not 100; the d-pad keeps its pace). On a list's last row on
view, a step further scrolls the list. The frame glides to the next target in about 70 ms (it jumps
when it was hidden or the move is over half the screen), and each hop ticks softly (the menu's move
sound: the first hop of a press always, a repeat while the d-pad or the stick is held at most every
150 ms; a step at a list's end that scrolls nothing is no hop and no tick). As a window opens or a tab turns, the frame starts on its item
cell nearest the top-left corner, or without cells on the target nearest it (a tab before the close
button). After RT turns a page or tab, the frame starts on that window's first target past its page
controls (the new page's first cell or button, not a tab: `PadFrame::SnapToPage`), never on another
window. It waits for the new page to show and lay out, at most 300 ms (pad round 8: `PadTargets::TurnShown`,
`PadTargetPick::HasPageTargets`); a page with nothing past its tabs (the guild's empty Alliance page) leaves it
on the turned-to tab, and a d-pad or stick move during the wait keeps the frame where the player took it; a window that opens within 0.75 s after, before the frame moved (an NPC shop, its goods
sent by the server after the inventory opened), takes it on the same way; when its target goes away, it
goes to the nearest one. While a message box or a menu over the windows is up (the item card),
the frame rests only on it. The right stick still moves the pointer (shown while off every target, at
half speed over a window) and snaps it onto a target near when let go. A legend under the hub's tabs
says what the buttons do there (on an item: Take, Equip, Card, Close, with X named for what it does
there: Use on a potion or a scroll, none on a jewel or anything else X does nothing to, Take off on worn gear, Sell while a shop is open, Move while the storage, the trade or the chaos
machine takes it; on an item in the chaos machine's box: Take, Move, Close; on a shop's goods: Buy,
Close; on an empty cell or a worn slot with nothing in it: Close alone; with an item on the cursor: Put down, Drop, Put back;
on a button: Press, Close; with the right stick's pointer on no target, no X or Y: A Press, B Close, or A
Repair in the bag's repair mode); both keep left of the windows, wrapping when the room is
short. On the Skills tab the legend gives way to the skill picker's own, and the picker sits under the
tabs:

| Pad | Over a window |
|---|---|
| A | Click at the frame: take an item, put it down, press a button, buy a shop's item |
| X | The item's own action at once: put on, use, move to the trade; on worn gear, take it off (the legend then says Take off). Never throws an item down. When it can't, it says why: the item's requirements, no room in the bag. A bag item whose place is taken swaps with the worn one (pad round 6, also the card's Equip): the worn item goes to a free bag cell first, then the new one goes on, each move after the server's answer (`UI/Inventory/EquipSwap`). With no free cell it says "No room in the bag to take off <item>"; it does not swap with the trade, a shop, the storage or the chaos machine open, while another move waits, or when the swap would take off two items (a two-handed weapon or a bow against the other hand's item: take that off first). A swap that stops halfway leaves both items in the bag and says so. In a shop it asks with the card first |
| Y | The item card (above; by the storage or the chaos machine its answer is Move); nothing on the chaos machine's own items, which have no card |
| RS | Right-click at the frame (on a stat's "+" the menu of bigger steps) |
| B | Back: closes the window on top; with an item on the cursor, puts it back where it was taken from instead |
| Y with an item on the cursor | Drops it to the ground under the hero (refused, as with the mouse, for expensive, bound and reinforced items) |
| LB, RB | The hub's tabs (the help's pages with the help open; over the help the legend hides, the help's last row says its keys) |
| RB (no hub, two or more windows open) | The frame goes to the next window, left to right and round, back to where it was in it (`PadFrame::NextWindow`); the legend says so |
| RT | The window's next page or tab: its next-page button, or the tab after the lit one, round (`PadTargets::TurnPage`, the markup's `pad-page` and `pad-page-next` classes); the legend says so, in the hub too when the hub's window has pages (the bag's tabs) |
| Menu | The menu of every window |

A dialog alone (a trade or party request) keeps the free pointer, and d-pad up and down over a list
that scrolls turn the wheel. The other buttons act as in the world.

**More windows** (pad round 8, `UI/HUD/PadWindowNames`, tested in `tests/ui/test_pad_window_names.cpp`). The
windows that place themselves take the frame too: the MU Helper's skill picker (A on a skill slot opens it; while
it is open the frame rests only on it, A picks a skill, B closes the list, and the frame goes back to the slot), the
friend list (RT turns its tabs), its letters (the name, the text and Send; the pad's keyboard types the text) and
chat rooms (since /play round 33 a friend's Chat opens a whisper, so a room shows only when a friend opens one).
Places only to look at carry the markup class `pad-look`: the frame rests on them as on a control, and the pointer
on their middle shows what a mouse over them shows (the battle pass's reward icons with their cards, its three day
chests and the week chest with their hints). The guild window's three tabs carry `pad-page`, so RT turns them.

On the login, server and character screens the gold frame rests on their controls as on the windows:
the d-pad and the left stick hop it, A presses there (a text field brings up the pad's keyboard), B
is Esc and Menu Enter; a screen's message box takes the frame alone. On the bare character screen a
legend at the top says the keys: LB and RB (or the d-pad left and right) choose the character, A
plays, X creates one, Y deletes the chosen one, Menu opens the menu (the server list is there), B is Esc.
With no control on view both sticks move the pointer and A or RT click. The pointer starts at the screen's centre the first time the pad takes over, so A
attacks at once; later it stays where the mouse left it, so a window under it stays under it (a
lifted finger leaves it off the window: then the centre again). A question (trade, party) takes Menu
as yes after a second, and A only once A has rested a second, so A tapped in a fight goes on
attacking. A button still held when the menu closes acts only once released, so the B that closed it does
not also press Back. In the world, a pad plugged in or pulled out says so in the system log ("Gamepad
connected", "Gamepad disconnected": `Gamepad::TakeNews`, read by `GamepadControls`); the pads found at
the start and those the Options switch opens say nothing, and in a browser a pad shows (and says so) at
its first press. Help lists
every button on a page of its own while the pad is in use (after the touch page with touch on too),
and LB and RB turn the pages.

**A lost connection** (pad round 8). The reconnect dialog (`UI/Dialogs/ReconnectDialog`) takes the frame on its
Cancel in every scene, the world too (`PadWindowNames::IsWorldDialog`); the pad's overlay is drawn over it then
(depth 50.5 over the dialog's 50: `RmlStackingOrder::PadOverlayOverReconnectDepth`). A presses Cancel, and B and
Esc cancel too. On the login, character and loading screens any Esc does: the system menu no longer opens over the
dialog (`CSceneUICoordinator::Update`) and the login window stays off (`CLoginWin`) while
`ReconnectManager::IsActive`. In the world only an Esc no window took cancels: the hot keys pass it on
(`UI::Reconnect::TakeEscape`), so an Esc that closes the bag or the chat line leaves the reconnect on; in the few
world frames where the windows don't update (a map loading) the mouse or A on Cancel does it. In a browser Esc
acts a few seconds late, as each connect attempt holds the page.

| Piece | File | Role |
|---|---|---|
| Device | `Core/Input/GamepadInput.{h,cpp}` | SDL gamepad events → held buttons, presses since the last frame and the sticks (radial dead zone, triggers as buttons), across up to four pads; which device the player used last. The sticks are also polled once a frame (`Poll`): SDL keeps the first value an axis reports as its rest and sends no event for it. |
| Pointer | `Core/Input/GamepadPointer.{h,cpp}` | The stick moves the SDL pointer (warped), buttons as pushed SDL mouse events with their own `which`; held buttons ORed into `IsKeyDown(VK_LBUTTON/VK_RBUTTON)`, the wheel too (`TurnWheel`, for the d-pad over a list; `Core::Input::UiScrollsAt` says whether a list there scrolls). A real mouse moving it is followed; a browser repeating a resting mouse's place (it does on layout changes) is not. |
| Mapping | `UI/HUD/GamepadControls.{h,cpp}` | Once a frame before the touch controls: presses their controls (`TouchControls::SetPadHeld`), moves the menu's highlight; after them (`LateUpdate`) steers (`GameLogic::Touch::Steer`); off the world, the plain pointer; with a text field focused, hands the pad to the keyboard. `HidesPointer` keeps `RenderCursor` from drawing the game's pointer. |
| Frame | `UI/HUD/PadFrame.{h,cpp}`, `PadTargets.{h,cpp}`, `PadTargetPick.{h,cpp}` | The targets are the visible elements of the game's windows (a workspace-placed document, or one drawn between depth 8 and the message boxes) that a click works (`drag: block` in base.rcss), `.item-cell` and `.equip-slot`, with the list that scrolls each; `PadTargetPick` chooses the one under the pointer, the nearest, and the next one a step points at. Tests: `tests/ui/test_pad_target_pick.cpp`. |
| Hub | `UI/HUD/PadHub.{h,cpp}` | The tabs and their windows (`g_pNewUISystem->Show/Hide`, the skill picker through `TouchControls::OpenSkillPicker`). |
| Overlay | `UI/HUD/PadOverlay.{h,cpp}` + `pad_overlay.rml` + `themes/{legacy,modern}/pad_overlay.rcss` | The frame, the hub's tabs and legend, the pickup prompt (`GameLogic::Touch::NearestDropToPick`, `DropOnScreen`; its name drawn by `NameWindow`). Depth 10.72: over the windows and message boxes, under the item on the cursor. |
| Card or action | `UI/Mobile/ClickSource.{h,cpp}` | `IsPadCardClick`: the pad's right button came from Y (`Pointer::SetCardClick`). |

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
| View | English letters, Russian ones, then (the chat line only) the quick chat phrases |
| LB, RB | Move the cursor (held, it repeats) |
| Menu | Enter: send the chat line, log in |
| Left stick click | On the chat's message line, the chat bar's next channel: All, Party, Guild, Gens, passing by the ones the hero is in none of (pad round 8, `UI/Chat/ChatChannelCycle`, tested in `tests/ui/test_chat_channel_cycle.cpp`); the keyboard names the channel a line goes to; with a name in the whisper field the line goes out as a whisper, so the keyboard names no channel and the click does nothing |

A key a pad button also presses carries that button's glyph. A password field gets English letters
only; a field that takes digits (`text-field--numeric`) gets a number pad. The guild's new name takes the pad's
keyboard too, and Esc while it is typed closes the window as with no field (pad round 8, `CGuildMakeWindow::TakesTypingFrom`). Buttons still held when
the keyboard closes act only once released, so the B that closed it does not also press Back.

**Quick chat phrases** (pad round 6). Twelve phrases: Hello! / Thanks! / Invite me to the party / Wait for me /
Help! / Let's go / Buff me, please / Selling, whisper me / Buying, whisper me / Sorry / Good game / Bye. Every
reader sees a phrase in their own language, in the chat log and over the head. On the pad keyboard's phrase page
(the chat's message line only, not the whisper name; View after the Russian letters) A sends the highlighted one at once; on a phone the chips
over the chat bar do the same. A phrase goes to the channel the chat bar is on (normal, party, guild) or to the
whisper target when one is filled in, with a 2 s pause shared with the macros (a phrase after a macro waits 2 s,
a macro after a phrase the macros' own 4 s), and not in Chaos Castle; a phrase refused for either plays the error sound. On the wire it is ordinary chat text after
the channel prefix: a token and the English text, `[#07] Buff me, please`; a client that knows the id shows its own
text, an older one (and the control socket's log) the English line. The server only relays it. Unknown ids show
as they came; a typed `[#07] anything` shows as the phrase. Mouse play has no phrase row; bots never use it
(`UI/Chat/ChatPhrases`, the texts `ChatPhrase01..12` in `UI/Chat/ChatPhraseTexts`, tested in
`tests/ui/test_chat_phrases.cpp`).

| Piece | File | Role |
|---|---|---|
| Layout | `UI/HUD/PadKeyboardLayout.{h,cpp}` | The pages (English, Russian, symbols, digits), their keys and widths, and moving between keys (rows centred, up and down to the nearest key's middle). Tests: `tests/ui/test_pad_keyboard_layout.cpp`. |
| Keyboard | `UI/HUD/PadKeyboard.{h,cpp}` + `pad_keyboard.rml` + `themes/{legacy,modern}/pad_keyboard.rcss` | Shows itself for the focused field (`GetFocusedTextField`), types into it through RmlUi's own SDL path (`Core::Input::RouteToUi`: text input and key events, as a real keyboard), Enter and Esc also as game keys (`Touch::TapKey`). Depth 35 in `RmlStackingOrder.cpp`: over the windows and messages of any scene, under the loading screen. It stands at the screen's foot, at its top for a field low on the screen, weighed again every frame; the login window steps up to the top while it is up (`CLoginWin::SetPosition`, `PadKeyboard::IsUp`), so both fields stay in view. |
| Repeats | `Core/Input/GamepadStepper.{h,cpp}` | The d-pad's and stick's steps with a first pause and a repeat (the stick's quicker after a long hold), shared with the menu's highlight; the stick's way with its lean in and out and its kept axis (`PointingOf`, tested in `tests/core/test_gamepad_stepper.cpp`); a held button's repeat. |
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
| Hero frame | `UI/Mobile/HeroFrame.{h,cpp}`, `UI/HUD/HeroGauges.{h,cpp}` | Fills the hero frame's part of the touch controls' model, writing only what changed (the potions are counted four times a second); HeroGauges reads HP/MP/AG/SD and EXP for it and for the PC bar alike. |
| Item card | `UI/Mobile/ItemCard.{h,cpp}`, `UI/Inventory/InventoryActionController.cpp`, `UI/Inventory/MyInventory.cpp`, `UI/Inventory/StorageInventory.cpp`, `UI/Inventory/StorageInventoryExt.cpp` | The card (a generic menu dialog); the controller opens it for a finger's long press or R3 on a bag item, the inventory for a tap, a long press or R3 on worn gear, the storages for a long press or the pad's Y on theirs. |
| Click source | `UI/Mobile/ClickSource.{h,cpp}` | Whether a window's click came from a finger's tap, drag or long press, or from the pad's R3 (`Touch::Pointer::LastLeftPress`), so that a window can ask first only for those. |
| Finger unit | `UI/Mobile/TouchUnit.{h,cpp}`, `UI/Mobile/ButtonSize.{h,cpp}` | The unit the controls are sized in and the Button size option (Options → Interface, `OptionWindow.cpp`). |
| Neighbours | `UI/Mobile/PhoneHudLayout.{h,cpp}`, sums in `UI/Mobile/PhoneHudFit.{h,cpp}` | Moves the buffs, pets' frames, system messages, skill list, party cards and chat beside the controls; tells the world labels where the HUD lies over them; keeps the notices under the open menu (`UI/Mobile/NoticesUnderPanel.{h,cpp}`: their own depth lies under every window, so it never raises them). Finds each document once (again every 250 ms while missing) and places a piece only when the controls, the screen or the piece's size changed, or every 250 ms. |
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
