# Options Window and Config

Behaviour changes that landed in PR
[#335](https://github.com/sven-n/MuMain/pull/335) for the options window
and the `config.ini` it writes to.

For the camera architecture see [`camera-system.md`](camera-system.md).

---

## What changed

The options window is now **the same window in every scene** (login,
character select, gameplay) - it shares one layout, one apply path, and one
backing config object. There is no separate per-scene options dialog.

What was added on top of that:

- **Resolution and windowed/fullscreen toggle apply without restarting.**
  Switch them at any time from the options window.
- **Volume and render-level sliders round to the nearest level** instead of
  truncating, so the slider endpoints actually reach 0 (mute) and the
  configured maximum.
- **Resolution combo-box clicks no longer leak through** to the checkboxes
  behind the dropdown.
- **Resolution changes go through SDL** (PR
  [#530](https://github.com/sven-n/MuMain/pull/530)), which makes the live
  apply actually work in windowed mode on Windows. Windowed resizes re-center
  the window on the display, except on Wayland, where the compositor places
  windows (see [Linux - Terminal](build/linux/console.md#wayland)).
  Fullscreen picks the closest real display mode (or borderless desktop when
  nothing matches), and the combo and `config.ini` record the mode that
  actually resulted, not the one requested.
- **The resolution list comes from the current monitor.** Opening the options
  window queries SDL for that display's fullscreen modes, collapses duplicate
  refresh-rate entries to one width/height choice, and sorts the result.
- **The UI reflows immediately after resolution changes.** Docked side menus
  scale up to 2.25 times their reference size on large displays, while
  centered dialogs remain capped at 2 times their reference size. Text grows
  with the active layout up to the dock scale, shrinking only when needed to
  fit an existing control.

## Layout

The window has five tabs:

| Tab | Settings |
|---|---|
| Display | Resolution, windowed mode, VSync, FPS limit, UI scale |
| Graphics | Quality: graphics preset, Enhanced graphics, anti-aliasing, FXAA, sharpening, anisotropic filtering, render level. Effects: the effect toggles. Shadows: the shadow toggles |
| Audio | Sound and music volume, whisper sound |
| Gameplay | Auto attack, slide help |
| Interface | Language, font, UI theme, FPS counter, debug info |

Each setting is a row with its name on the left and its control on the
right, aligned in two columns. A tab's settings scroll when they don't fit,
so a tab can take more rows.

- **Legacy theme:** the window keeps the same size on every tab, so it does
  not move when you switch tabs. A long setting name, for example in a
  translation or with a larger font, wraps to a second line. On a screen too
  short for the window, it shrinks to fit and the settings scroll below the
  tab row. The small cross in the top-right corner closes it. The window can
  be dragged by any part that is not a control.
- **Modern theme:** a full-screen settings page with the tabs on the left.
  The page scrolls when its rows don't fit.

Rows with a hint (UI Scale, Anti-aliasing, Anisotropic filtering and the
Enhanced tuning sliders) show it while the pointer is over the row.

## What it stores in `config.ini`

Settings are written next to the executable as `config.ini`. The options
window writes to these sections:

- `[Window]` - width, height, windowed flag. A new installation ships with
  `Width=0` and `Height=0`: the first start opens a windowed game window of the
  largest display mode of the main monitor that fits its usable area (the
  screen without the taskbar) and writes those numbers; existing values are not
  touched. The browser build takes its size from the page.
- `[Render]` - set from the Display and Graphics tabs, each applied at once:
  - `VSync`: `$vsync on` and `$vsync off` update it too; startup and
    fullscreen/resolution changes reapply it before the next frame begins.
    With VSync on the display paces the frames and the game sets no limit of its own. If the
    frame rate still runs far past the fastest display's (a display asleep or switched off, a
    remote session, VSync forced off in the driver), the game caps it at that display's rate (60 FPS at least),
    144 FPS when the rate is unknown, writes one warning to the log and looks every 15 seconds
    whether VSync paces again (`Scenes/FramePacing.h`, `VsyncWatch`).
  - `FpsCap`: the frame limit while VSync is off, `-1` for none.
  - `EffectLevel` (0 to 4) and `FullEffects`: the effect level bar and its "Render full effects" checkbox.
  - `Look` (default `Classic`): the "Enhanced graphics" checkbox, ticked for
    the Enhanced look; see [`enhanced-look.md`](enhanced-look.md).
  - `Antialiasing` (`0`, `2`, `4`, `8`; default `4`): the Anti-aliasing row,
    the samples of the Enhanced look. Classic always renders without it.
  - `Anisotropy` (`1` is off, up to `16`; default `8`): the Anisotropic
    filtering row, for every texture with mip levels: world textures in the
    Enhanced look, upscaled textures in both.
  - `EnhancedLighting`, `DynamicLights`, `LightMood`, `PostProcessing`
    (default `1`): the checkboxes of the rows under "Enhanced graphics", each a
    feature of the Enhanced look; see [`enhanced-look.md`](enhanced-look.md).
    Clicking a row's name opens its tuning sliders instead.
  - `DisableEffects`, `DisableParticles`, `DisableSkillEffectModels`,
    `DisableBoids`, `DisableBodyShadows`, `DisableWingShadow`: the Graphics
    tab's toggles. A toggle another one already covers shows ticked and
    greyed out: Disable Effects covers skill effects and wildlife; without
    Render full effects, or with Disable Shadows, no wing shadow is drawn
    (and without full effects no shadow at all).
  - `SoftShadows` (default `1`): the Soft shadows row under Shadows, browser
    build only (the desktop hides it). Blurs the body shadows; greyed out and
    off when no body shadow is drawn. `$softshadow <strength%> <levels>` tunes
    it for the session (0.65 and 1 by default). Cape and cloth shadows stay hard.
  - `Ssao` and `SsaoStrength` (defaults `1` and `100`): the **SSAO (contact shadows)** row and the
    SSAO strength slider under it (0 to 100 % in steps of 10, shown while SSAO is on), Remaster PC
    client only (the browser and Classic clients hide them). Both apply at once. The row is greyed
    out, its hint saying why, without HD textures, on the Low graphics preset, and without Enhanced
    lighting and Bloom and colour grading; when SSAO was off as the game started, turning it on
    takes effect after a restart (the hint says so). See [`enhanced-look.md`](enhanced-look.md).
  - `Wind` and `WindStrength` (defaults `1` and `100`): the **Wind (swaying grass and trees)** row
    and the wind strength slider under it (0 to 100 % in steps of 10, shown while the wind is on),
    Remaster PC client only (the browser and Classic clients hide them). Both apply at once. The row
    is greyed out, its hint saying why, without HD textures and on the Low graphics preset. See
    [`enhanced-look.md`](enhanced-look.md).
  - `SmoothAnimation` (default `1`): the **Smooth animation** row, Remaster PC client only (the
    browser and Classic clients hide it). It applies at once. `AnimationFadeMs` (default `160`,
    `0` to `300`) has no row. See [`enhanced-look.md`](enhanced-look.md).
  - `Haze` and `HazeStrength` (defaults `1` and `70`): the **Distance haze** row and the haze
    strength slider under it (0 to 100 % in steps of 10, shown while the haze is on), Remaster PC
    client only (the browser and Classic clients hide them). Both apply at once. The row is greyed
    out as the SSAO row is, its hint saying why; when both the haze and SSAO were off as the game
    started, turning it on takes effect after a restart. See [`enhanced-look.md`](enhanced-look.md).
  - `Particles` and `ParticlesStrength` (defaults `1` and `70`): the **Air particles** row and the
    particle density slider under it (0 to 100 % in steps of 10, shown while the particles are on),
    Remaster PC client only (the browser and Classic clients hide them). Both apply at once. The row
    is greyed out as the Wind row is, without HD textures and on the Low graphics preset, its hint
    saying why. See [`enhanced-look.md`](enhanced-look.md).
  - `TorchLights` and `TorchLightsStrength` (defaults `1` and `70`): the **Torch light on the ground**
    row and the torch light strength slider under it (0 to 100 % in steps of 10, shown while it is
    on), Remaster PC client only (the browser and Classic clients hide them). Both apply at once.
    The row is greyed out without HD textures, on the Low graphics preset and without Enhanced
    lighting with Dynamic lights, its hint saying why. See [`enhanced-look.md`](enhanced-look.md).
  - `FoliageSmoothing` and `FoliageSmoothingStrength` (defaults `1` and `70`): the **Smooth foliage
    edges** row and the foliage smoothing strength slider under it (0 to 100 % in steps of 10, shown
    while it is on), Remaster PC client only (the browser and Classic clients hide them). Both apply
    at once. The row is greyed out without HD textures and on the Low graphics preset, its hint
    saying why. See [`enhanced-look.md`](enhanced-look.md).
  - `SunShadowSoftEdges` and `SunShadowSoftEdgesStrength` (defaults `1` and `70`): the **Soft sun
    shadow edges** row right under the Sun shadows row and the shadow edge softness slider under it
    (0 to 100 % in steps of 10, shown while it is on), Remaster PC client only (the browser and
    Classic clients hide them). Both apply at once. The row is greyed out without HD textures, on
    the Low graphics preset and with Sun shadows off, its hint saying why. See
    [`enhanced-look.md`](enhanced-look.md).

  Set by hand only, never written by the game:
  - `Backend` (`default`/`vulkan`/`direct3d12`/`metal`, case-insensitive,
    `d3d12` accepted as an alias for `direct3d12`) forces a specific SDL_gpu
    GPU backend at startup, falling back to SDL's own auto-pick with a logged
    warning if the requested one is unavailable.
  - `SortParticleDraws` (`0`/`1`) sorts particle draws back to front.
  - `PreloadMonsters` (`0`/`1`, default `1`), PC client only: on a map change
    the monster types the server spawns there (`Data/Local/MapMonsters.txt`)
    are loaded behind the loading screen instead of when each first shows.
- `[EnhancedTuning]` - only the tuning sliders under "Enhanced graphics" the
  player moved, one global key per value applying on every map and after restart;
  no per-map player setup is needed. "Defaults" means no personal override;
  Reset removes the key again. Written on its
  own, through a temporary file, when a slider is let go; see "Tuning it
  yourself" in [`enhanced-look.md`](enhanced-look.md).
- `[UI]` - `Locale`, `Font`, `RmlTheme` and `UIScalePercent` from the Display and
  Interface tabs. `EnableAnimationTaskPool` (`auto` by default, `0`/`off`,
  `1`/`on`; set by hand only, like the `--enable-taskpool` and
  `--disable-taskpool` command-line flags) animates characters on worker
  threads, and `ReuseCharacterBones` (`1` by default, set by hand only) lets
  render keep the character bones built earlier in the frame.
- `[Audio]` - volumes.
- `[Login]` - language, and the remembered-credential keys (`RememberMe`,
  `SavePassword`, `EncryptedUsername`, `EncryptedPassword`); see
  "Remembering login credentials" below.
- `[ConnectionSettings]`
- `[Camera]` - orbital wheel-zoom radius (`Zoom`). **New in this PR**, the
  only key the camera rework added.
- `[KeyBindings]` - the player's keys (see "Key bindings" below): one line per
  action that is not on its default key, for example `Inventory=B` or
  `MuHelperToggle=X`. An action on its default key has no line, so the section
  is empty or missing until a key is changed, and the template lists none.

Missing keys fall back to compile-time defaults from
`src/source/Data/GameConfig/GameConfigConstants.h`. If `config.ini` doesn't exist,
the build seeds it from `src/bin/config.ini.template`, which lists every key the
game writes, in the order `Save()` writes them, plus `Backend`.

## Key bindings

Options > Gameplay has a Key bindings row (in the game only, and not on phones and
tablets, which have no keyboard to press the new key with) that closes the
settings and opens the key bindings window (`UI/Options/KeyBindingsWindow.h`,
`key_bindings.rml`, both themes). It lists every action with its key, in
three groups:

- **Windows:** Inventory (I) and its second key (V), Character (C), Party (P),
  Guild (G), Friends (F), Quests (T), Teleport (M), Menu (U), the command
  window (D), Commands (J), Events (O), Daily rewards (N), Battle pass (X),
  Gens (B), MU Helper settings (Z) and MU Helper on/off (Home).
- **Potions:** the four potion slots (Q, W, E, R).
- **Skills:** the ten skill slots (1 to 9, then 0).

A click on a row waits for a key; the next key pressed becomes the action's
key. The bindable keys are the letters but A, H, K, L, S and Y (kept for the
game's own uses), the digits and Home: exactly as many keys as actions, so
every bindable key belongs to one action, and a key another action holds
swaps the two (the other row flashes). Escape cancels the wait; Enter, Tab,
Space, the F keys, the arrows and the reserved letters are refused with a
hint and the wait goes on. A second click on the waiting row also cancels.
Reset to defaults gives every action its default key back. Every change is
saved to `[KeyBindings]` at once. The keys inside windows (Enter, Esc, Tab,
the chat's keys, Shift with a digit for pet commands) stay fixed.

The rules live in `Core/Input/KeyBindings.h` (tested in
`tests/core/test_key_bindings.cpp`); `Core/Input/KeyBindingsRuntime.h` holds
the player's table, loaded once at start right after `GameConfig::Load()`. A
hand-edited section that binds one key twice or names an unknown key is
repaired on load and written back. The hot keys, the potion and skill slot
labels on the HUD, the tooltips that name a key, the touch buttons and the
pad's shortcuts all follow the bindings. The bindings are per computer (per
browser on /play), not per character.

## Graphics preset

The Graphics tab starts with a **Graphics preset** row (Low, Medium, High,
Ultra; "Custom" when the values match none), and has **FXAA** and **Sharpening**
rows next to Anti-aliasing. The presets, the `GraphicsPreset`, `Fxaa` and
`Sharpen` keys and how Auto picks a preset from the GPU are described in
[`enhanced-look.md`](enhanced-look.md#graphics-presets-and-screen-filters).

Under the preset both desktop clients (Classic and Remaster) have a **Suggest a lower preset when
FPS is low** checkbox (`[Render] LowFpsHint`, on by default): the low frame rate hint, which offers
the next lower preset when the game has run really slowly for a while. Its "Don't ask again" button
turns the checkbox off; checking it turns the hint back on. The browser client doesn't show the row.
See [`enhanced-look.md`](enhanced-look.md#low-frame-rate-hint).

The preset, FXAA, sharpening and **Sun shadows** rows are in both desktop clients (sun shadows
wherever the renderer built their pipelines). In the Classic client (no `Data\HD` folder,
`Render::Textures::Hd::Installed`) the presets' Medium, High and Ultra rows sharpen 30 % instead of
40 %, and a new install keeps `config.ini`'s own values (the original look, FXAA, sharpening and
sun shadows off; the preset reads Custom) instead of picking a preset by GPU. The first start of a
Classic client that predates this turns FXAA, sharpening and sun shadows off once (older Classic
clients forced them off every start) and saves `[Render] ClassicGraphicsReset=1`, so a value a
Remaster run left in `config.ini` doesn't come back on by itself; after that the player's choices
stay. The Remaster client never writes that line. **HD textures** and the Remaster's own effects
(SSAO, wind, haze, particles, torch light, foliage smoothing, soft sun shadow edges, HD texture size)
stay Remaster-only: the Classic client hides them and keeps them off
(`Render::Look::RemasterGraphicsOptions`). OpenMU's own rows are in both. The browser client shows
the rows as before.

Under HD textures the Remaster client has an **HD texture size** row (Auto / Full / Half,
`[Render] HdTextureSize`), shown only while HD textures are on; a change applies after a restart.
Half loads the HD copies at half their sides, a quarter of the texture memory, and Auto picks it on
integrated graphics and on a PC with less than 12 GB of memory: see
[`enhanced-look.md`](enhanced-look.md). The browser client never shows it.

The Remaster's smoother models (`[Render] SmoothModels`, default 1;
`Render/Models/ModelSmoothing.h`) have no Options row. They follow the HD textures: on in the
Remaster client, always off in the Classic client (`Winmain.cpp`), off in the browser.

The Remaster has no normal maps: they were removed on 2026-11-18 (ghinzuro: barely visible in
game; the Remaster pack is 362 MB smaller and starts about 23 s faster). An old `config.ini` line
`NormalMaps=1` is ignored, and `scripts/hd_pack.py` deletes old `.nrm` files from its output.

## Remembering login credentials

The login screen has two separate checkboxes:

- **Remember Username** - fills in your account name next time. Safe to leave
  on; only the username is stored.
- **Remember Password** - also stores the password. Because a saved password
  lets anyone using the same computer log into your account, ticking it opens a
  confirmation dialog first; the box only stays ticked if you accept it. Leave
  this off on shared machines (e.g. an internet cafe).

Editing the account or password field clears any stored credentials and turns
Remember Password back off, so an out-of-date password is never left behind.

Stored credentials are obfuscated (not plaintext) and tied to the current
machine and user account, so copying `config.ini` to another machine does not
reveal them. This is protection against a casual reader of the file, not a
determined attacker - which is why saving the password is opt-in.
