# Theming & Modding

How to edit and select themes, use shared design tokens, and supply custom assets. The client
ships `legacy` and `modern`. Additional theme folders work without recompilation and can inherit an
existing theme.
A separate user/mod override layer is not implemented.

## Shared markup and theme-specific structure

Start with a window's shared RML and `legacy` stylesheet to understand its compatibility layout.
The C++ bindings define the required ids, model fields and event callbacks; the theme defines
their presentation. Keep shared markup theme-neutral and use RCSS when the same structure serves
both themes. A theme-specific RML override is appropriate when a different composition cannot
reasonably use the shared structure. Preserve the behavioral contract in either case.

The shared file stays theme-neutral: theme-specific class vocabulary (`modern-frame`,
`modern-panel`, …) belongs in that theme's fork, never in the shared `.rml` (`login.rml`,
`msg_win.rml` and `remember_password_prompt.rml` are modern's earliest forks for this reason).

**Forking safely needs a check.** Run `python tools/check_rml_rcss_drift.py` before shipping
theme edits; it also runs during the build. Each theme must provide the literal ids the window
looks up and callbacks its view registers, including linked templates and shared-document
fallback. Fields offered by a model must appear in at least one of its document/theme variants,
so different full/current readouts remain legal. Comments and quoted expression text cannot
satisfy a missing name. The report includes document/theme/file counts; zero discovery fails.

Use `--review` to inspect discovered names and intentional omissions with their reasons. A
control intentionally absent from a variant can carry
`<!-- rml-contract-optional-id: name: reason -->` or
`<!-- rml-contract-optional-callback: name: reason -->`, after confirming its C++ path tolerates
the omission. Unused model fields need a `// rml-contract-unused: name: reason` comment in the
owning source. Exceptions fail when their names disappear from the discovered contract or become
used again; repair required controls instead of exempting them.

The guard checks literal source contracts, not actual visibility, class styling, model-name
correctness or interactions. Dynamic ids, selectors, struct members and arbitrary registration
helpers require manual review and running-client checks.

## How theme resolution works

A theme is identified by **folder name**, not a closed C++ enum, specifically so adding one
(including a fully programmatic, sprite-free one) is normally a zero-source-change,
drop-a-folder operation. Reopen Options after adding or removing a theme to refresh its list;
the list also refreshes after switching themes.

`config.ini`'s `[UI] RmlTheme=<name>` selects the active theme, read once at startup by
`GameConfig::GetRmlTheme()` and cached by `UI::RmlBridge::GetActiveThemeName()`. That cache is
also live-mutable via `UI::RmlBridge::SetActiveThemeName()` — see
["Switching themes without relaunching"](#switching-themes-without-relaunching) below. A
missing or invalid saved theme falls back to `legacy`, with a diagnostic in the error log. If
`legacy` is also unavailable, restore the bundled theme files before launching. Loading a
window goes through its `ThemedView`, which hands `"Data/Interface/RmlUi/login.rml"` to
`UI::RmlBridge::ThemedDocumentLoader::Load()`:

1. It reads `login.rml` from the active theme, then its ancestors, then the shared markup.
2. It builds a synthetic source URL, `Data/Interface/RmlUi/themes/<name>/login.rml` — this path
   need not exist on disk.
3. It calls `Rml::Context::LoadDocumentFromMemory(text, syntheticSourceUrl)`.
4. RmlUi resolves the document's `<link href="login.rcss"/>` relative to that synthetic URL,
   landing on `Data/Interface/RmlUi/themes/<name>/login.rcss`. The file can come from that theme
   or an ancestor. Its `token(name)` markers use the selected theme's merged tokens; relative
   links and assets keep the selected theme's virtual path so child overrides apply throughout.

This is confirmed against RmlUi's own source: `Context::LoadDocumentFromMemory` wraps the string
in a `StreamMemory` and calls `SetSourceURL()` on it before parsing; `XMLNodeHandlerHead.cpp`'s
`MakeExternalResource()` resolves `<link href>` via `AbsolutePath(path, parser->GetSourceURL())`.

Every migrated window renders its chrome through RmlUi, in every theme — no native sprite path
draws underneath it. A "legacy-look" theme reproduces the
original art by pointing its own RCSS decorators at the same image files the old sprites used;
a "modern" theme uses flat colors/vector shapes instead.

## Forking a theme's RML

`UI::RmlBridge::ThemedDocumentLoader::Load()` ([`RmlTheme.cpp`](../../src/source/UI/RmlBridge/RmlTheme.cpp))
looks for `themes/<theme>/<name>.rml` first, then each ancestor, then the shared `<name>.rml`.
`modern` forks a
few dozen documents; `themes/modern/*.rml` is the live list. Most of these exist so `modern`'s own `.modern-frame`/`.modern-frame-crimson`/`.title-glow` class vocabulary
doesn't have to live in the shared, theme-neutral file (the Shared markup section above).

`main_frame.rml` is an example of shared markup with different presentation. Legacy shows the
original bottom button row; modern uses `main_frame_top.rml` and flat backing elements.
The shared document carries every part either
theme uses, and each theme's `main_frame.rcss` hides the others (`display: none`). Where the two
themes bind different values to the same spot, the markup carries both variants and each theme
shows one: a gauge's readout as `.value-current` or `.value-full`. The document is also split into parts (`.hud-part`: the strip backing, each
gauge group, EXP, buttons, item hotkeys, skill row, skill list), each placed by its own rule in
the theme's `main_frame.rcss` with its contents laid out from its own corner, so a theme moves
or hides a part without touching the others. The skill list's grid fans out from its part's box,
the first cell's position, so it docks wherever the theme puts that box.

**Fork the RML only when a theme needs markup the shared file cannot reasonably carry** — prefer
adding the element to the shared file and letting the other theme hide it. A fork's
ids/classes/bindings have to be kept in step with the shared file by hand;
`tools/check_rml_rcss_drift.py` catches missing required literal ids/callbacks and unused model
fields; visual structure and interaction still need testing in the client.

## Adding a theme folder

For development builds, keep themes under `src/bin/Data/Interface/RmlUi/themes/`.
Asset staging replaces the runtime RmlUi directory with this source tree, so a theme added
only beside `Main.exe` is removed on the next asset copy. A theme folder with only a `theme.ini`
that inherits `legacy` (as below) should look identical to `legacy`, including the login scene;
`tests/ui/test_theme_file_interface.cpp` checks this with such a folder of its own.

A custom theme can contain only the resources it changes. For example, a `classic` theme can
inherit `legacy` by putting this in `themes/classic/theme.ini`:

```ini
[Theme]
Base=legacy

[Meta]
DisplayName=Classic
```

Resolution is **child → declared ancestors**, with shared markup as the final document fallback.
RML, templates, RCSS and theme-local images use the first matching whole file. An overridden
stylesheet replaces its ancestor's file; rules are not automatically combined. Paths to common
assets outside the theme folder keep their ordinary meaning.

`[Tokens]`, `[Capabilities]` and `[Fonts]` inherit individual keys. A child value wins, including
`NativeTextSize=0` or an empty font entry. Display names stay specific to the child. Names use
letters, digits, `_` and `-` and are normalized to lowercase. Missing ancestors, invalid names
and inheritance cycles make a theme unavailable for selection.

1. Create `src/bin/Data/Interface/RmlUi/themes/<your-theme-name>/`.
2. Declare `Base=legacy` or `Base=modern` in `[Theme]`, or supply your own complete stylesheets.
   Every window links `base.rcss` for shared rules such as buttons, checkbox states, backdrops
   and the `body { pointer-events: none; }` reset. It must be available in the theme's chain.
3. Override only the `.rcss` or `.rml` files you want to change, preserving the required ids and
   bindings. Without a base, supply every stylesheet the markup links; a missing sheet leaves
   that window partly unstyled. Run the contract guard before testing in the client.
4. Select it in **Options → UI → Theme**, edit `config.ini`'s `[UI]` section:
   `RmlTheme=<your-theme-name>`, or preview it live without
   editing anything — type `$theme <your-theme-name>` in chat (see
   ["Switching themes without relaunching"](#switching-themes-without-relaunching) below).
5. Relaunch (if you edited `config.ini`). No rebuild needed either way — this is a pure data/config
   change.

## Switching themes without relaunching

`$theme <name>` (typed into the chat box, handled locally by `CmuConsoleDebug::CheckCommand` —
never sent to the server, same bucket as `$fps`/`$vsync`) hot-swaps the active theme for the
current session: `$theme modern`, `$theme legacy`, or any modder-supplied folder name.

Mechanically: it validates the inheritance chain and a readable `base.rcss`, supplied by the
theme or an ancestor (`UI::RmlBridge::ThemeExists()`) —
an unknown name is rejected with no state change, not left to fail silently per-window — then
calls `UI::RmlBridge::SetActiveThemeName()` to update the live cache and
`UI::RmlBridge::ReloadAllThemedDocuments()` to invoke every registered theme-reload callback.
Every themed document belongs to a `UI::RmlBridge::ThemedView` (`component-catalog.md`'s Theming
section), which registers for theme switches the first time it builds. On a switch each view keeps
its model's values, unloads its documents, loads them again from the active theme and shows again
the ones that were visible; views rebuild from the bottom of their context's stack up, so the
stacking survives. A window that was never opened has nothing to rebuild and picks up the active
theme when it first builds. The free-function modules (the tooltip, notices, the remember-password
prompt) hold a namespace-scope view and work the same way.

**The chat command is session-only.** `$theme` does not write to `config.ini`. For a selection
that survives a restart, use **Options → UI → Theme** or edit `[UI] RmlTheme` while the client
is closed. The Options control uses `UI::Theme::Select(..., Persistence::Saved)` and rebuilds
documents after the click dispatch has finished.

**The maintained first-party themes are `legacy` and `modern`.** Custom folders use the same
loading mechanism. The architecture policy's Custom/Test-theme checks seek to expose visual
coupling; the current project uses modern's structural differences and the contract guard for
that coverage. Update both built-in themes when window content or behavior changes.

### `#backdrop` usage

```html
<div id="backdrop"></div>
<div id="panel" data-model="...">
    ...
</div>
```

Placed as the *first* child of `<body>`, before the panel, so it paints underneath. Unlike the
panel's children, `#backdrop` sets `pointer-events: auto` directly (not inherited) — a full-screen
dim backdrop is meant to consume clicks underneath it (matching how a full-screen legacy dialog
already treated the whole screen as "in this window"), not create a click-through hole.

## Bringing your own images to a theme

A theme is free to reuse the original game art or bring entirely new image assets (a custom
background, custom button art, a logo). Both go through the same path: any image an RCSS file
references loads through this engine's normal texture pipeline
(`RmlUiRenderInterface::LoadTexture` → `CGlobalBitmap::LoadImageExclusive`). No engine changes are needed
to use it — but there are real constraints worth knowing first.

**Path resolution**: a relative image path inside an RCSS rule resolves against *that
stylesheet's own location*, not the shared `.rml`'s synthetic per-theme URL — so a custom theme's
image can simply sit next to its own `.rcss`:

```
themes/<your-theme-name>/
    login.rcss
    panel_bg.tga      <- referenced from login.rcss below
```

```rcss
#panel {
    decorator: image( panel_bg.tga );
}
```

**Only this engine's proprietary OZT/OZJ containers are actually read, not plain PNG/JPG/BMP.**
`CGlobalBitmap::LoadImage` only recognizes two extensions, `.jpg`/`.tga` — and **both internally
swap the extension to `.OZJ`/`.OZT` before opening.** Concretely: if your RCSS references
`panel_bg.tga`, the file that actually needs to exist on disk is `panel_bg.OZT`, not a real
`.tga`. There is no PNG/JPG→OZT/OZJ converter in this repository — producing one needs an
external tool from the wider MU private-server modding community (the format predates this
project).

**RmlUi file images load at their original dimensions**, including non-power-of-two images.
Use `@spritesheet` when selecting a rectangle from an atlas, or when a named sprite makes the
stylesheet clearer:

```rcss
@spritesheet panel-bg-sheet
{
    src: panel_bg.tga;
    panel-bg-image: 0px 0px 329px 245px;  /* the image's REAL, unpadded pixel size */
}

#panel {
    decorator: image(panel-bg-image);   /* the sprite name, not "panel_bg.tga" directly */
}
```

File images and atlas sprites support `repeat`, `repeat-x` and `repeat-y` in this fork.
Repeating an atlas sprite generates geometry per tile; see
[Engine Findings](engine-findings.md#rmlui-build) for the behavior and remaining limits.

**Failures are completely silent.** A missing file or unsupported extension returns `false` from
`LoadImage` with no log line at all — the element just renders with no image, no error, no
diagnostic. If a custom theme's image isn't showing up, check the extension-swap rule above (a
real `.tga` file, or a file with the wrong internal container, both fail exactly the same silent
way) before assuming something else is wrong.

Plain-image loading is not implemented for theme assets. A custom theme must supply the
containers described above.

## Reforge: recolouring the game's own sprites

A theme can give the game's interface art other colours without shipping any image: its
`theme.ini` lists sprites in a `[Reforge]` section, each with a preset, and the client recolours
the decoded pixels as RmlUi loads the texture, before the upload. The original and its HD copy
(Remaster) go through the same preset, scaled to the copy's size, so both look alike. The bundled
`skynet` theme (`Base=legacy`) is built this way: legacy's documents, stylesheets and art, forged.

```ini
[Reforge]
newui_msgbox_back=stone
newui_item_table01(l)=metal-flat
partcharge1/newui_menu03=gild
```

- **Key**: the sprite's path under `Data/Interface/`, lower-case, without its extension
  (`.tga`/`.OZT` and `.jpg`/`.OZJ` all match). `../../../newui_msgbox_back.jpg` in a stylesheet is
  `newui_msgbox_back`; `Data/Interface/partCharge1/newui_menu03.OZJ` is `partcharge1/newui_menu03`.
  A theme's own art (under `Data/Interface/RmlUi/themes/`) is never reforged.
- **Presets** (`Render/Textures/SpriteReforge.h`): `metal` (frames: dark iron, antique gold on the
  light curls, deeper relief, a warm glow), `metal-flat` (the same colours without relief or glow,
  for thin and tiling pieces and buttons), `metal-bright` (`metal-flat` lifted, for buttons with a
  light icon such as the HUD's menu buttons, so the icon stays readable), `cell` (the inventory
  lattice: dark bronze lines), `stone` (window bodies, lit from the top), `socket` (equipment
  slots), `gild` (the HUD's current-skill box: `metal`, its green frame gold and its green fill
  deep bronze, so a skill icon on it stands out) and `none`. They recolour the grey parts only
  (`cell` recolours all, `gild` its green too): saturated pixels (coloured gems, painted marks)
  keep their colour, and `metal` and `stone` also keep bright text.
  The blurred parts (`metal`'s relief and glow, `stone`'s softening) are worked out on the
  original's grid and spread over an HD copy, so a copy at four times the size costs little more
  than one pass over its pixels.
  A `metal` piece that shows a seam where it meets its neighbour (its relief and glow blur within
  the sprite) goes to `metal-flat`.
- **Inheritance**: the section merges down the chain like any other; a child replaces a parent's
  entry and `none` turns one off. An unknown preset name is logged once in `MuError.log` and the
  sprite is drawn as it is.
- **Scope**: only textures RmlUi loads. A window still drawn by native code keeps the original
  colours, since RmlUi loads its own copy of each sprite (`LoadChangedImageExclusive`).
- **Theme switch**: RmlUi keeps a texture by its source path, which is the same in every theme, so
  `UI::Theme::Select` releases the sprites whose preset changes at once (no frame draws them
  with the old preset); RmlUi loads them again, with the new preset, when it next draws them. No
  restart.
- **Colours that go with the art** stay tokens: `skynet/tokens.ini` sets the window titles
  (`window-title-color`, `window-title-effect`), the window shadow (`window-shadow`), the item cell
  tint (`cell-tint-normal`), the character window's stats (`stat-accent`) and `text-heading`;
  `legacy/tokens.ini` holds the literals those stylesheets had, so legacy draws as before.

Code: `UI/RmlBridge/ThemeReforge.h` (keys, the list, the switch), the `textureImageChange` hook
(`RmlRuntimeHooks.cpp` → `RmlUiRenderInterface::LoadTexture`), `CGlobalBitmap::ImageChange`.
Tests: `tests/render/test_sprite_reforge.cpp` (`sprite_reforge_tests`) and
`tests/ui/test_theme_reforge.cpp` (`theme_file_interface_tests`); the latter also checks that
every `skynet` key names a sprite that exists. `sprite_reforge_preview` (built with the tests)
forges one raw RGBA image, to look at a preset on real art without starting the game.

## Rich item tooltips and rarity cells

Two `theme.ini` capabilities change how items look, with no image of their own. `skynet` declares
both; legacy and modern declare neither, so their documents, classes and pixels stay as they were.
C++ reads the capability, never the theme's name.

```ini
[Capabilities]
RichItemTooltip=1
ItemRarityCells=1
```

- **`RichItemTooltip=1`**: the item tooltip (`RenderItemInfo` → `UI::Tooltip::ShowLegacyTextList`)
  keeps writing the same lines, a few with a role (`TextListRole`: name, Combat Power, section,
  inactive detail, set piece). With the capability on and outside the phone view,
  `UI::Tooltip::ArrangeRich` (`UI/Tooltip/ItemTooltipLayout.h`) reorders them: a header of the
  name, the item's kind ("Ancient · Armour", the loot filter's words) and its Combat Power; each
  run of blank lines as one divider; a divider before each section; ✓ / – before the set pieces.
  While Shift is not held, three or more inactive set lines (grey Power of the Ancients and full
  set bonus lines) give way to a "Hold Shift for details" line; touch and the pad always show
  everything. `Tooltip::Config` then carries `rich`, the rarity tier and the upgrade band, and the
  model offers `rich`, `name_px`, `rar_*`, `up_gold` and per line `role_*`, `head`, `head_last`.
  The theme forks `tooltip.rml` to bind them and styles them in its own sheet
  (`themes/skynet/tooltip_rich.rcss`). The line divs must stay `#tooltip_panel`'s first children;
  extra elements (skynet's gold frame and corner diamonds) go after them. The rarity is the name
  colour's (`GameLogic/Items/ItemRarity.h`): ancient, excellent, socket, +7, options, special,
  divine; the band is +7..+12 or +13..+15, for weapons, shields, armour and wings only.
- **`ItemRarityCells=1`**: every native item grid's cells and the worn slots also carry
  `rar-exc` / `rar-anc` / `rar-socket`, `rf` (has a frame), `up-fine` / `up-gold` and, on a grid,
  the item's outer edges `re-t` / `re-b` / `re-l` / `re-r` (`UI/Inventory/ItemCellRarityClass.h`).
  Only gear and jewellery get them. `themes/legacy/item_grid.rcss` and `my_inventory.rcss` style
  them with the `cell-tint-*` and `cell-frame-*` tokens, which are neutral in
  `legacy/tokens.ini` (no frame, the normal tint); a theme sets its own values. The rarity tint
  replaces only `tint-normal`, so wear and untradeable tints still show, and the frame is the
  drop preview element's border, so the drop colours are untouched.

A rich tooltip taller than the window is laid out again with smaller text (`text_px`, `name_px`,
the line heights and the padding, down to 3/4) until it and its shadow and glow fit, in the same
frame; still too tall, `#tooltip_panel` gets `tt-tall` (model `tall`), which skynet uses to drop
the shadow and the gold glow. The +13..+15 cell and slot frame is `cell-frame-gold-width` wide
(skynet 2px, neutral 0px), the other frames `cell-frame-width`.

Tests: `tests/gamelogic/test_item_rarity.cpp`, `tests/ui/test_item_tooltip_layout.cpp`,
`tests/ui/test_item_cell_classes.cpp`, and in `tests/ui/test_theme_reforge.cpp` the capabilities,
the neutral tokens and the layout of skynet's rich tooltip (header bars, dividers, the Combat
Power pill, the diamonds and the gold frame).

## Live HUD

`LiveHud=1` makes the PC HUD live (`UI::RmlBridge::ThemeHasLiveHud()`, `UI::HUD::LiveHud` in
`UI/HUD/LiveHud.h`). `skynet` declares it; legacy and modern do not, so their HUD keeps its
documents, classes and pixels. C++ reads the capability, never the theme's name.

```ini
[Capabilities]
LiveHud=1
```

- **What it draws.** Life and mana gems filled with liquid: an eased level, a wave and slosh, a
  crest, a glass highlight and a sheen; a damage ghost on the life gem; a halo that pulses while
  life is low. SD and AG rings round the gems, the SD one with its own ghost. On the skill row a
  dark sweep over the cooldown left, the whole seconds left, and a short gold flash when a slot is
  ready again. On the EXP bar a spark breathes at the fill's end and flares on each EXP gain; a
  level-up sends a band of light across the bar and the sheen over both gems.
- **Markup.** `main_frame.rml` is shared by every theme. Its live parts carry `live-only`: per gem
  `orb_hp_*` / `orb_mp_*` (`_box`, `_under`, `_halo`, `_body`, `_shine`), the rings
  `orb_sd_arc` / `orb_ag_arc` and their `_cap`, per skill slot `.skill-sweep`, `.skill-ready` and
  `.skill-cd-secs`, and `#exp_spark`. The meshes are `<additive-mesh>` elements; C++ builds their
  geometry (`UI/HUD/LiveGaugeMesh.h`) at most every 1/30 s, and only when what they show changed.
- **`main_frame_live.rcss` in every theme.** `main_frame.rml` links it, so a theme without a
  `Base=` must ship one. Legacy's and modern's hold only `.live-only { display: none; }`. Skynet's
  shows and places the parts, hides the old cooldown wipe on the skill row (the open grid keeps
  it), animates `.skill-ready` on the slot's icon while C++ gives the slot the class `ready` (0.4 s:
  a 2dp rim fading out and a faint additive fill), draws `.skill-cd-secs` smaller while C++ gives it
  the class `long` (10 s and more), and hides the parts again under `@media (theme: mobile-hud)`.
- **Phones.** A phone or tablet always plays legacy (`ResolveStartupThemeName`), and the phone's
  HUD (`UI::Mobile::ReplacesPcHud()`) never syncs the live HUD, so there is none there.

`[LiveHud]` in `theme.ini` tunes it (`UI::HUD::LiveGauge::LiveHudConfig`). Every key is optional;
a missing value, or one out of range or not a number, keeps its default. Do not put a comment after
a value: `ThemeInheritance` keeps it, and the value is refused. Sizes are dp, times seconds, angles
degrees clockwise from 3 o'clock, colours `#rrggbb` (`SweepShade` `#rrggbbaa`).

| Key | Default | Meaning |
|---|---|---|
| `GemOutline` | `25,3 75,3 96,32 96,70 68,97 32,97 5,70 5,32` | the gem's convex outline, x,y in percent of its box |
| `GemZoom` | 1.32 | the gem art's zoom about its centre (cuts off its corner curls), at least 1 |
| `EmptyShade` | 0.30 | the art's brightness above the liquid |
| `CrestTint` | 0.30 | the crest's brightness at the liquid's surface |
| `WaveAmplitude` | 1.2 | the surface wave's height |
| `WavePeriodLife`, `WavePeriodMana` | 2.6, 3.3 | the wave's period in each gem |
| `ArcStart`, `ArcSweep`, `ArcWidth` | 140, 260, 3.5 | the SD and AG rings: start angle, extent, width |
| `LowLife`, `PulsePeriod`, `PulseStrength` | 0.30, 1.2, 0.22 | the life gem pulses under this share of life |
| `HaloLife`, `HaloMana`, `HaloPoison` | `#ff3418`, `#2870ff`, `#30c040` | the glow round each gem |
| `SweepShade` | `#060403cc` | the skill cooldown's shade |
| `RimOuter`, `RimInner`, `RimWidth` | `#5c462a`, `#0b0907`, 1.5 | the socket's rim round each gem; the colours also draw the rings' track |
| `SparkRadius`, `SparkPeak` | 5, 0.6 | the EXP spark's size and its brightness (0..1) at the top of its breathing |
| `SparkPeriod`, `SparkStill` | 1.4, 0.825 | the breathing's cycle; the brightness with HudEffects off, a share of `SparkPeak` |
| `FlareTime`, `FlareRadius`, `FlareBrightness` | 0.8, 1.8, 1.5 | an EXP gain's flare: how long it takes to fall back, how many times as big and as bright the spark starts (at least 1) |
| `BandTime`, `BandHalfWidth`, `BandStrength` | 0.9, 16, 0.8 | the level-up: the run of the band and the gems' sheen, the band's reach either side of its line, its brightness (0..1) |

The colours drawn by the stylesheet are `hud-*` tokens in the theme's `tokens.ini`:
`hud-orb-plate` (the plate over the strip's sockets round the bigger gems), `hud-sd-num` and
`hud-ag-num` (the SD and AG numbers), `hud-ready-rim` (the ready flash's rim), `hud-ready-flash`
(the colour the flash adds over the slot at its start) and `hud-cd-secs` (the seconds left).
Legacy and modern have none.

**HUD animation.** Options > Interface "HUD animation" (`config.ini` `[UI] HudEffects`, on by
default) shows only while the live HUD runs. Off: the liquid lies flat, and there is no sheen, no
low-life pulse, no ready flash and no level-up cue; the spark stands still at `SparkStill` without
its flare. The levels still ease, the ghosts still drain, and the cooldowns still show their sweep
and seconds.

**`$hudtest`** (chat, `UI/HUD/LiveHudTest.h`) puts local values in place of the hero's, to show
and screenshot every state; nothing goes to the server, and leaving the game scene clears it all.
A form it does not know answers with the usage line.

| Command | Effect |
|---|---|
| `$hudtest hp\|mp\|sd\|ag <0..1>` | a gauge's fill |
| `$hudtest expfill <0..1>` | the EXP bar's fill (within its tenth), and with it the spark |
| `$hudtest cd <seconds>` | every skill slot cooling down for that long, from now; `cd 0` stops it |
| `$hudtest exp` | an EXP gain (the spark's flare) |
| `$hudtest lvl` | a level-up (the band and the sheen) |
| `$hudtest poison on\|off` | the hero poisoned or not (the life gem's poisoned art and halo) |
| `$hudtest stats` | one system line with the live HUD's cost over the last full 5 s window |
| `$hudtest stats on\|off` | that line also logged at every full window (`MU_LOG_INFO`: `MuError.log`, and the browser console on the web) |
| `$hudtest off` | the hero's own values again, and the stats log off |

The stats line reads `HUD stats over 5.0 s: 60.0 syncs/s, 30.0 ticks/s; mesh rebuilds per tick avg
4.0, max 10; sync avg 42 us, max 900 us`: `LiveHud::Sync()` calls per second, the syncs past the
1/30 s gate, the meshes rebuilt per such tick, and each sync's own time (binding left out). Before
the first window is full it shows the running one; where no live HUD runs it says so.

Tests: `tests/ui/test_live_gauge_math.cpp` and `tests/ui/test_live_gauge_mesh.cpp` (the motion,
`[LiveHud]` parsing, `$hudtest`, the stats window and the meshes), and in
`tests/ui/test_theme_reforge.cpp` the capability, skynet's `[LiveHud]` values and the live parts'
layout in each theme and on the phone.

## Design tokens: `themes/<theme>/tokens.ini`

Both built-in themes have a `tokens.ini` file (`[Tokens]` section, plain `name=value` lines) that a theme's own `.rcss`
files reference via a `token(name)` marker instead of repeating a literal value everywhere it's
used. RmlUi follows each document's ordinary external `<link>` and loads the themed `.rcss`
through the game's file interface. That interface resolves `token(name)` from the selected
theme's `tokens.ini`, then its ancestors, before RmlUi parses and caches it: plain text substitution, not a CSS
feature. CSS custom properties (`var()`) and `calc()` work too, for a value derived from another
inside the theme (the item grids' `--item-cell`, `item_grid.rcss`). A new theme needs only its own
files; it does not need a C++ change. The stylesheet cache is cleared when a theme is selected
again, so editing theme files and reselecting that theme refreshes their values.

If a stylesheet refers to a token absent from the entire chain's `[Tokens]` sections, the client logs
the token name and stylesheet path in `MuError.log`. The missing value still expands to an empty
string. Add the key to `tokens.ini` and reselect the theme to reload the stylesheet.

**What a token is for**: a *reusable, theme-level semantic choice* — the standard body text color,
a shared muted/secondary text tier, the common tooltip backing, a shared accent/highlight, a
warning/danger color, shared scrollbar track/thumb colors, a shared corner-radius. The test isn't
"does this literal appear more than once" — it's "does changing this value represent one coherent
design decision a theme author would actually want to make in one place." `legacy`'s own
`tokens.ini` documents, token by token, which real recurring selector(s) motivated it; a token with
no real call site backing it (invented for taxonomy-completeness alone) doesn't belong.

**What should stay ordinary RCSS, not a token**:
- **One-off decorative colors** — a single window's own specific accent choice with no cross-window
  or cross-selector reuse. Most of a theme's literal colors are legitimately this; not every color
  needs a lever.
- **Gameplay colours** — gauge gradient stops around a base fill, the cooldown wipe, a status
  colour such as poisoned HP or server load, and white-outlined text over the 3D world. They mean
  something in the game, not in the theme.
- **Content-driven/asset-driven palettes** — `tooltip.rcss`'s `.tt-blue`/`.tt-red`/`.tt-yellow`/etc.
  rich-text colors (and their `.tt-hl-*` background-highlight pairs) are the exact RGB values
  `RenderTipTextList()`'s native `TEXT_COLOR_*` switch (`ZzzInventory.cpp`) already used — they
  identify a *content type* (an item's rarity tier, a warning message), not a theme aesthetic
  choice, and both `legacy`'s and `modern`'s own copies of this file deliberately leave them as
  literals with a comment saying so. A ninepatch/gradient decorator recipe tightly bound to one
  specific sprite composition (`server_select.rcss`'s own button-tint overlays) is the same
  category — asset-specific, not a theme palette lever.
- **Structural geometry** — panel width/height, row height, pixel offsets, icon pitch, spritesheet
  rects, native-companion-synchronized positions (anything [`README.md`](README.md)'s Known limits records as an exception). A value becoming
  `token(foo)` does not make duplicated geometry architecturally themeable — that requires actually
  deriving the position live (or, where that's currently impractical, an explicit pinned
  cross-reference comment on both sides). Tokens are a color/typography/radius mechanism, not a substitute
  for the geometry-decoupling work tracked separately.

Each theme's own token meanings: `themes/modern/README.md`; `themes/legacy/tokens.ini`'s comments.

**Cross-theme naming**: reuse a `modern` token's NAME for a `legacy` token when the semantic ROLE
genuinely matches (`text-primary`, `font-body`, `radius-sm` all exist in both, with each theme's
own appropriate value — `legacy`'s `text-primary` is `#ffffff`, `modern`'s is a warm off-white).
Don't force a shared name onto two concepts that don't actually match, and don't rename an existing
token in one theme purely for cosmetic cross-theme symmetry — a real naming inconsistency is worth
fixing, a theme's own genuinely distinct concept (an accent family the other theme doesn't have)
just gets its own name.

## Optional system fonts

List font files in `theme.ini`'s `[Fonts]` section, with one file per key. For example:

```ini
[Fonts]
body=%WINDIR%\Fonts\verdana.ttf
body-bold=%WINDIR%\Fonts\verdanab.ttf
```

Select the loaded family in `tokens.ini`:

```ini
[Tokens]
font-body=Verdana
font-title=Verdana
```

Paths support `%ENVIRONMENT_VARIABLE%`, absolute filenames, and paths relative to the theme
that declared the entry. Missing files or unresolved variables are skipped. Inherited font
entries retain their declaring directory; a child can replace a key or leave its value empty
to disable it. Load each style you need, such as regular and bold.

`font-*` tokens are family names. If the requested family is unavailable, the client uses the
first available family for that token in an ancestor, then bundled Liberation Sans. The bundled
CJK glyph fallbacks remain loaded. Custom literal `font-family` rules must still name a loaded
family; use the tokens for optional system fonts.

The player's font choice in the options (`[UI] Font`) overrides `font-body` and `font-title` in
every theme, as long as RmlUi has that family loaded. With `Default`, the theme's own families
apply. Other `font-*` tokens always keep the theme's value. A new choice applies right away: every
themed document is rebuilt with it, the same way as after a theme switch.

`font-native` always names the family the native text renderer uses (the `Font` choice, and
DejaVu Sans for `Default`). It is meant for text RmlUi draws inside a box the native renderer
measured, such as the world labels. Themes do not set it.

Fonts load before themed documents at startup and on theme selection. RmlUi retains loaded
families for the session, so a family loaded by an earlier theme remains available. Reselecting
a theme retries optional files; restart the client to replace a font file whose family is
already loaded. Referencing a player's installed font does not package it with the theme.

## Coordinates, scaling, and positioning — what a theme actually controls

### Scaling: a global user setting, opt-in per element

**See [Layout, Anchoring & Scaling](layout-and-scaling.md) for the full policy** — condensed here:
`[UI] UIScalePercent` (default 100) contributes to
`Context::SetDensityIndependentPixelRatio()` through the shared scale hook. A coordinate written
in `dp` uses the context's ratio, which combines viewport fit, OS content scale and the user
setting. A `px` length bypasses that ratio but still follows any ancestor transform. Use `dp`
for directly laid-out scalable content, reference `px` inside transformed panels, and
counter-scaled physical text where the native typography policy applies. Raster assets follow
their element's drawn size; choose their scaling deliberately.

### Panel position vs. element layout vs. draggability — three different owners

- **Layout of elements *within* the panel** — expressed in RCSS, which the theme controls. A few
  windows still take an image file or texel rect from C++ (CryWolf's event art; see
  [README.md](README.md#known-limits)), so a theme
  can rearrange that art but not replace it.
- **Where windows and HUD parts go** — the theme's `workspace.rml`/`workspace.rcss`: regions and
  one slot per window or HUD part, laid out in RCSS; the game places each open window on its slot
  ([window-placement.md](window-placement.md)). What a theme can do there:
  - move a slot to another region, or reorder slots, to change the arrangement — including the
    main HUD, chat, header corners and event HUDs;
  - mark a shell region `data-participation="reserve"` (windows avoid it) or `overlay`;
  - list in a slot's `data-closes` the windows that close for lack of room when it opens;
  - give a slot `data-fit="fill"` and a size in `%`, for windows that support it;
  - cap a dock region with `max-height: 100%` so its windows scale down to fit.

  Worked examples (split docks, centred inventory, a side bar, chat on the right, the HUD at the
  top) are in window-placement.md's "Theme recipes". Windows without a slot keep the rule below.
- **The panel's own position on screen** for a window without a slot — its own RCSS anchoring
  (`.center-both`, `position: absolute; right: 0; bottom: 0;`, …), or, for the few that follow
  the pointer or a target (help, item explanations, quick command, tooltips), C++.
- **A button's hint** — markup: `data-hint` (or a bound `data-attr-data-hint`) on the element. The
  shared tooltip places it and `tooltip.rcss` styles it, so a fork that moves or adds a button
  keeps its hint with no C++.
- **Draggability** — `UI::RmlBridge::MakeDraggable()`, wired per window in C++; RCSS has no
  "make this draggable" property. Which windows drag is decided in
  [window-placement.md](window-placement.md) section 8.

## Known limitations

- **The `$theme` command is session-only.** `$theme <name>` (see
  ["Switching themes without relaunching"](#switching-themes-without-relaunching) above) rebuilds
  every open window's document in place, but doesn't persist to `config.ini` — a relaunch still
  uses whatever's saved there. The model survives the rebuild, so a window needs no resync of its
  own; what it sized or placed from the old theme's metrics goes in its view's `afterReload`.
- **Custom theme images require the engine's proprietary OZT/OZJ format, not plain PNG/JPG** —
  see [Bringing your own images to a theme](#bringing-your-own-images-to-a-theme) above. No
  converter tool exists in this repo, and a missing/wrong-format image fails silently.
- **Every themed document belongs to a `ThemedView`**, so `$theme` covers all of them; a new
  window declares one.
- **Theme identity must never drive C++ branching** — `architecture-principles.md` §30. Where a
  theme must change C++ behaviour, it declares a capability in `theme.ini` (`NativeTextSize` is the
  one in use). Prefer removing the need: the main frame's `ProvidesOwnIconChrome` existed only so
  C++ could skip drawing legacy chrome for the modern theme, and went away once that chrome was
  RCSS in the legacy theme's own stylesheet.
- **RmlUi renders last in the frame**, so native content that must sit *in* a window's stacking
  goes into a `UI::RmlBridge::RenderTarget` the document shows: live 3D and a window's remaining
  native 2D alike, so every document is in the one RmlUi context.
