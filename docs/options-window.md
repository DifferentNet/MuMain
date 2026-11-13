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
  - `Haze` and `HazeStrength` (defaults `1` and `70`): the **Distance haze** row and the haze
    strength slider under it (0 to 100 % in steps of 10, shown while the haze is on), Remaster PC
    client only (the browser and Classic clients hide them). Both apply at once. The row is greyed
    out as the SSAO row is, its hint saying why; when both the haze and SSAO were off as the game
    started, turning it on takes effect after a restart. See [`enhanced-look.md`](enhanced-look.md).

  Set by hand only, never written by the game:
  - `Backend` (`default`/`vulkan`/`direct3d12`/`metal`, case-insensitive,
    `d3d12` accepted as an alias for `direct3d12`) forces a specific SDL_gpu
    GPU backend at startup, falling back to SDL's own auto-pick with a logged
    warning if the requested one is unavailable.
  - `SortParticleDraws` (`0`/`1`) sorts particle draws back to front.
- `[EnhancedTuning]` - only the tuning sliders under "Enhanced graphics" the
  player moved, one global key per value applying on every map and after restart;
  no per-map player setup is needed. "Defaults" means no personal override;
  Reset removes the key again. Written on its
  own, through a temporary file, when a slider is let go; see "Tuning it
  yourself" in [`enhanced-look.md`](enhanced-look.md).
- `[UI]` - `Locale`, `Font`, `RmlTheme` and `UIScalePercent` from the Display and
  Interface tabs. `EnableAnimationTaskPool` (`0`/`1`, set by hand only, like the
  `--enable-taskpool` command-line flag) animates characters on worker
  threads.
- `[Audio]` - volumes.
- `[Login]` - language, and the remembered-credential keys (`RememberMe`,
  `SavePassword`, `EncryptedUsername`, `EncryptedPassword`); see
  "Remembering login credentials" below.
- `[ConnectionSettings]`
- `[Camera]` - orbital wheel-zoom radius (`Zoom`). **New in this PR**, the
  only key the camera rework added.

Missing keys fall back to compile-time defaults from
`src/source/Data/GameConfig/GameConfigConstants.h`. If `config.ini` doesn't exist,
the build seeds it from `src/bin/config.ini.template`, which lists every key the
game writes, in the order `Save()` writes them, plus `Backend`.

## Graphics preset

The Graphics tab starts with a **Graphics preset** row (Low, Medium, High,
Ultra; "Custom" when the values match none), and has **FXAA** and **Sharpening**
rows next to Anti-aliasing. The presets, the `GraphicsPreset`, `Fxaa` and
`Sharpen` keys and how Auto picks a preset from the GPU are described in
[`enhanced-look.md`](enhanced-look.md#graphics-presets-and-screen-filters).

These rows, **HD textures** and **Sun shadows** are MU Skynet's and belong to the Remaster
client: the Classic client (no `Data\HD` folder, `Render::Textures::Hd::Installed`) hides them
and keeps FXAA, sharpening, sun shadows and HD textures off whatever `config.ini` says, and Auto
picks no preset there (`Render::Look::RemasterGraphicsOptions`). OpenMU's own rows stay in both.
The browser client shows them as before.

Under HD textures the Remaster client has an **HD texture size** row (Auto / Full / Half,
`[Render] HdTextureSize`), shown only while HD textures are on; a change applies after a restart.
Half loads the HD copies at half their sides, a quarter of the texture memory, and Auto picks it on
integrated graphics and on a PC with less than 12 GB of memory: see
[`enhanced-look.md`](enhanced-look.md). The browser client never shows it.

The Remaster's smoother models and normal maps (`[Render] SmoothModels`, default 1, and
`NormalMaps`, default 1; `Render/Models/ModelSmoothing.h`, `Render/Textures/NormalMaps.h`) have
no Options row. They follow the HD textures: on in the Remaster client, always off in the
Classic client (`Winmain.cpp`), off in the browser. The normal maps ship prebuilt next to the
HD textures as `<texture>.nrm` at half size, two grey lossy WebP pictures (a map that would miss
`hd_pack.py --check`'s limits goes at a higher quality or lossless; `scripts/hd_pack.py`,
`Render/Textures/NormalMapFile.h`); the game only reads them, and a texture without one gets no
relief. For development, the environment variable `MU_NORMAL_MAPS_RUNTIME=1` builds missing
ones at load instead.

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
