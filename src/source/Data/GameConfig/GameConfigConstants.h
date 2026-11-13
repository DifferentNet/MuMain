#pragma once

namespace CfgSections
{
    inline constexpr wchar_t CfgSectionWindow[]     = L"Window";
    inline constexpr wchar_t CfgSectionGraphics[]   = L"Graphics";
    inline constexpr wchar_t CfgSectionAudio[]      = L"Audio";
    inline constexpr wchar_t CfgSectionUI[]         = L"UI";
    inline constexpr wchar_t CfgSectionLogin[]      = L"LOGIN";
    inline constexpr wchar_t CfgSectionConnectionSettings[] = L"CONNECTION SETTINGS";
    inline constexpr wchar_t CfgSectionCamera[] = L"Camera";
    inline constexpr wchar_t CfgSectionRender[] = L"Render";
    // MU Moldova: the loot highlight filter (GameLogic::Items::LootFilter).
    inline constexpr wchar_t CfgSectionLootFilter[] = L"LootFilter";
}

namespace CfgKeys
{
    // Window
    inline constexpr wchar_t CfgKeyWidth[]      = L"Width";
    inline constexpr wchar_t CfgKeyHeight[]     = L"Height";
    inline constexpr wchar_t CfgKeyWindowed[]   = L"Windowed";

    // Audio — volume 0 = off, >0 = on (no separate Enabled flag).
    inline constexpr wchar_t CfgKeySoundVolume[]  = L"SoundVolume";
    inline constexpr wchar_t CfgKeyMusicVolume[] = L"MusicVolume";
    inline constexpr wchar_t CfgKeyBackgroundSound[] = L"BackgroundSound";

    // Login
    inline constexpr wchar_t CfgKeyRememberMe[]        = L"RememberMe";
    inline constexpr wchar_t CfgKeySavePassword[]      = L"SavePassword";
    inline constexpr wchar_t CfgKeyLanguage[]          = L"Language";
    inline constexpr wchar_t CfgKeyEncryptedUsername[] = L"EncryptedUsername";
    inline constexpr wchar_t CfgKeyEncryptedPassword[] = L"EncryptedPassword";

    // Connection
    inline constexpr wchar_t CfgKeyServerIP[]   = L"ServerIP";
    inline constexpr wchar_t CfgKeyServerPort[] = L"ServerPort";

    // UI
    inline constexpr wchar_t CfgKeyUILocale[] = L"Locale";
    inline constexpr wchar_t CfgKeyFont[]     = L"Font";
    // Active RmlUi theme name ("legacy"/"modern"), Data/Interface/RmlUi/themes/<name>/. Not yet
    // a full runtime hot-swap -- read once at startup. See UI::RmlBridge::RmlTheme.
    inline constexpr wchar_t CfgKeyRmlTheme[] = L"RmlTheme";
    // Global RmlUi UI scale, as a percentage (100 = normal). Drives
    // Rml::Context::SetDensityIndependentPixelRatio(). Only RCSS authored in `dp` units responds
    // to this; existing `px`-authored windows are unaffected until retrofitted.
    inline constexpr wchar_t CfgKeyUIScalePercent[] = L"UIScalePercent";
    // MU Moldova: names over heads (UI::OverheadNames), set in the options window's Interface tab.
    inline constexpr wchar_t CfgKeyOverheadPlayerNames[] = L"OverheadPlayerNames";
    inline constexpr wchar_t CfgKeyOverheadNpcNames[] = L"OverheadNpcNames";
    // MU Moldova: title plates over the players (UI::TitlePlates).
    inline constexpr wchar_t CfgKeyOverheadTitles[] = L"OverheadTitles";
    // MU Moldova: the title plates' light, sparkles and glow (UI::TitlePlates::Effects).
    inline constexpr wchar_t CfgKeyTitleEffects[] = L"TitleEffects";
    // MU Moldova: titles only over players in towns (safe zones), UI::TitlePlates::ShownHere().
    inline constexpr wchar_t CfgKeyTitlesTownsOnly[] = L"TitlesTownsOnly";
    // MU Moldova: the phone or PC layout ("auto", "pc", "phone") and the controls ("auto", "mouse",
    // "touch"), set in the options window's Interface tab. See UI::Mobile::InterfaceMode.
    inline constexpr wchar_t CfgKeyInterfaceView[] = L"InterfaceView";
    inline constexpr wchar_t CfgKeyControls[] = L"Controls";

    // LootFilter: on/off, the beam, the drop sound, one digit per category (LootFilter::SerializeCategories),
    // the rarity mask, the +level and zen thresholds, and the names as Name0..NameN with NameCount.
    inline constexpr wchar_t CfgKeyLootFilterEnabled[] = L"Enabled";
    inline constexpr wchar_t CfgKeyLootFilterMarker[] = L"Marker";
    inline constexpr wchar_t CfgKeyLootFilterSound[] = L"Sound";
    inline constexpr wchar_t CfgKeyLootFilterCategories[] = L"Categories";
    inline constexpr wchar_t CfgKeyLootFilterRarity[] = L"Rarity";
    inline constexpr wchar_t CfgKeyLootFilterMinLevel[] = L"MinLevel";
    inline constexpr wchar_t CfgKeyLootFilterZenMin[] = L"ZenMin";
    inline constexpr wchar_t CfgKeyLootFilterNamePrefix[] = L"Name";
    // MU Moldova: the on-screen controls' size in percent (Options -> Interface, Button size).
    inline constexpr wchar_t CfgKeyTouchButtonSize[] = L"TouchButtonSize";
    // MU Moldova: whether the phone's top row of buttons is open (its toggle button, UI/HUD/TouchControls).
    inline constexpr wchar_t CfgKeyTouchTopRow[] = L"TouchTopRow";
    // MU Moldova: whether the game reads a gamepad (Options -> Interface, Gamepad).
    inline constexpr wchar_t CfgKeyGamepad[] = L"Gamepad";
    // MU Moldova: whether the pad vibrates on hits and on death (Options -> Interface, Gamepad).
    inline constexpr wchar_t CfgKeyGamepadRumble[] = L"GamepadRumble";
    // Animates characters on worker threads when many are in view. Off unless set here or
    // started with --enable-taskpool; manual config.ini edit only.
    inline constexpr wchar_t CfgKeyEnableAnimationTaskPool[] = L"EnableAnimationTaskPool";

    // Camera
    inline constexpr wchar_t CfgKeyZoom[] = L"Zoom";
    // MU Moldova: the camera the player left (CameraManager): F9 mode (0 default, 1 orbital),
    // F10 wheel lock, the default camera's wheel step and the orbital camera's turn and tilt.
    inline constexpr wchar_t CfgKeyCameraMode[] = L"Mode";
    inline constexpr wchar_t CfgKeyZoomLock[] = L"ZoomLock";
    inline constexpr wchar_t CfgKeyZoomLevel[] = L"ZoomLevel";
    inline constexpr wchar_t CfgKeyOrbitYaw[] = L"OrbitYaw";
    inline constexpr wchar_t CfgKeyOrbitPitch[] = L"OrbitPitch";

    // Render
    inline constexpr wchar_t CfgKeySortParticleDraws[] = L"SortParticleDraws";
    inline constexpr wchar_t CfgKeyVSync[] = L"VSync";
    // The look preset: Classic or Enhanced (texture filtering, Enhanced lighting,
    // dynamic model lights, the light mood at full strength and post-processing).
    // A missing or invalid value is the default.
    inline constexpr wchar_t CfgKeyLook[] = L"Look";
    // Anisotropic filtering of every texture with mip levels: world textures in the Enhanced
    // look, upscaled textures in both looks. 1 is off.
    inline constexpr wchar_t CfgKeyAnisotropy[] = L"Anisotropy";
    // Multisample anti-aliasing of the Enhanced look: 0 (off), 2, 4 or 8 samples per pixel.
    inline constexpr wchar_t CfgKeyAntialiasing[] = L"Antialiasing";
    // The Enhanced look's features the player keeps on; Classic ignores them.
    inline constexpr wchar_t CfgKeyEnhancedLighting[] = L"EnhancedLighting";
    inline constexpr wchar_t CfgKeyDynamicLights[] = L"DynamicLights";
    inline constexpr wchar_t CfgKeyLightMood[] = L"LightMood";
    inline constexpr wchar_t CfgKeyPostProcessing[] = L"PostProcessing";

    // The graphics preset: Auto, Low, Medium, High, Ultra or Custom (Render/Look/GraphicsPreset.h).
    inline constexpr wchar_t CfgKeyGraphicsPreset[] = L"GraphicsPreset";
    // Screen filters of the finished 3D scene: FXAA (0/1) and sharpening in percent (0 to 100).
    inline constexpr wchar_t CfgKeyFxaa[] = L"Fxaa";
    inline constexpr wchar_t CfgKeyHdTextures[] = L"HdTextures";
    inline constexpr wchar_t CfgKeyHdTextureSize[] = L"HdTextureSize";
    inline constexpr wchar_t CfgKeySmoothModels[] = L"SmoothModels";
    inline constexpr wchar_t CfgKeyNormalMaps[] = L"NormalMaps";
    // Grain on the ground near the camera, with HD textures on (desktop only): 0 off, 1 self-detail,
    // 2 noise (Render/Terrain/TerrainDetail.h), and its strength in percent (0 to 100).
    inline constexpr wchar_t CfgKeyTerrainDetail[] = L"TerrainDetail";
    inline constexpr wchar_t CfgKeyTerrainDetailStrength[] = L"TerrainDetailStrength";
    // [Render] TerrainVariation: with HD textures on, 0 off, 1 macro colour, 2 macro colour and
    // mirrored blocks (Render/Terrain/TerrainVariation.h), and its strength in percent (0 to 100).
    inline constexpr wchar_t CfgKeyTerrainVariation[] = L"TerrainVariation";
    inline constexpr wchar_t CfgKeyTerrainVariationStrength[] = L"TerrainVariationStrength";
    // [Render] TerrainWater: with HD textures on, 0 off, 1 ripples, sun highlight and sky tint on the
    // terrain's water tiles (terrain_water.frag), and its strength in percent (0 to 100).
    inline constexpr wchar_t CfgKeyTerrainWater[] = L"TerrainWater";
    inline constexpr wchar_t CfgKeyTerrainWaterStrength[] = L"TerrainWaterStrength";
    // [Render] Ssao: with HD textures on, 0 off, 1 soft contact shadows (screen-space ambient occlusion)
    // in creases and where models meet the ground, and its strength in percent (0 to 100).
    inline constexpr wchar_t CfgKeySsao[] = L"Ssao";
    inline constexpr wchar_t CfgKeySsaoStrength[] = L"SsaoStrength";
    // [Render] Wind: with HD textures on, 0 off, 1 grass, bushes and tree crowns sway in the wind
    // (Render/Models/WindSway.h), and its strength in percent (0 to 100).
    inline constexpr wchar_t CfgKeyWind[] = L"Wind";
    inline constexpr wchar_t CfgKeyWindStrength[] = L"WindStrength";
    // Sun shadows of the Enhanced look: 0 off, 1 medium (2048 map), 2 high (4096 map). The browser
    // build keeps them under their own key: older /play builds saved the presets' value in SunShadows,
    // and a browser player gets sun shadows only from the Options row.
#ifdef __EMSCRIPTEN__
    inline constexpr wchar_t CfgKeySunShadows[] = L"WebSunShadows";
#else
    inline constexpr wchar_t CfgKeySunShadows[] = L"SunShadows";
#endif
    inline constexpr wchar_t CfgKeySharpen[] = L"Sharpen";

    inline constexpr wchar_t CfgKeyRenderBackend[] = L"Backend";
    // DXP-23's per-system effect-cost toggles (MainScene.h's SetDisableEffects/SetDisableParticles/
    // SetDisableSkillEffectModels/SetDisableBoids/SetDisableWingShadow), originally console-only
    // ($effects ...) diagnostics -- promoted to persisted, in-game-options-exposed settings since
    // they're functionally just "reduce rendering cost for a low-end PC."
    inline constexpr wchar_t CfgKeyDisableEffects[] = L"DisableEffects";
    inline constexpr wchar_t CfgKeyDisableParticles[] = L"DisableParticles";
    inline constexpr wchar_t CfgKeyDisableSkillEffectModels[] = L"DisableSkillEffectModels";
    inline constexpr wchar_t CfgKeyDisableBoids[] = L"DisableBoids";
    inline constexpr wchar_t CfgKeyDisableWingShadow[] = L"DisableWingShadow";
    // Every body shadow: characters, their wings and pets, objects and wildlife.
    inline constexpr wchar_t CfgKeyDisableBodyShadows[] = L"DisableBodyShadows";
    // Soft body shadows (the browser build): the flat shadows blurred at the edges.
    inline constexpr wchar_t CfgKeySoftShadows[] = L"SoftShadows";
    // User-chosen FPS cap when VSync is off; -1 = uncapped. Same promotion story as the toggles
    // above, this time for the console-only `$fps <N>` diagnostic.
    inline constexpr wchar_t CfgKeyFpsCap[] = L"FpsCap";
    // Options' effect level bar (0..4) and its "Render full effects" checkbox. They lived only for one
    // run, so a player on a weak PC had to turn the effects down again after every start.
    inline constexpr wchar_t CfgKeyEffectLevel[] = L"EffectLevel";
    inline constexpr wchar_t CfgKeyFullEffects[] = L"FullEffects";
}

namespace CfgDefaults
{
    inline constexpr int  CfgDefaultWindowWidth  = 1024;
    inline constexpr int  CfgDefaultWindowHeight = 768;
    inline constexpr bool CfgDefaultWindowed     = true;

    inline constexpr int  CfgDefaultSoundVolume = 5;
    inline constexpr int  CfgDefaultMusicVolume = 5;
    inline constexpr bool CfgDefaultBackgroundSound = true;

    inline constexpr bool CfgDefaultRememberMe = false;
    inline constexpr bool CfgDefaultSavePassword = false;
    inline constexpr wchar_t CfgDefaultLanguage[] = L"Eng";
    inline constexpr wchar_t CfgDefaultEncryptedUsername[] = L"";
    inline constexpr wchar_t CfgDefaultEncryptedPassword[] = L"";

    inline constexpr wchar_t CfgDefaultServerIP[] = L"localhost";
    inline constexpr int CfgDefaultServerPort = 44405;

    inline constexpr int CfgDefaultCameraMode = 0;    // CameraMode::Default
    inline constexpr bool CfgDefaultZoomLock = true;  // the wheel doesn't zoom until F10
    inline constexpr int CfgDefaultZoomLevel = 3;     // DefaultCamera PLAYER_ZOOM_LEVEL_DEFAULT
    inline constexpr int CfgDefaultZoom = 1735;  // OrbitalCamera DEFAULT_RADIUS — matches Default-cam camera-to-Hero distance

    // I18N locale code; "en" is the default the resx generator falls back to.
    inline constexpr wchar_t CfgDefaultUILocale[] = L"en";

    // UI font family name. Empty = each platform's built-in default (Tahoma on
    // Windows, fontconfig "sans-serif" on Linux), so the look is unchanged until
    // the user picks a font. Any value is passed through as the GDI face name.
    inline constexpr wchar_t CfgDefaultFont[] = L"";

    // "legacy" reproduces the pre-RmlUi look with programmatic RCSS and no sprite dependency.
    // Defaults to legacy so nothing changes for existing players until they opt in.
    inline constexpr wchar_t CfgDefaultRmlTheme[] = L"legacy";

    // 100% = unscaled. The options window's UI tab offers a fixed ladder of values
    // (UI::Options::UIScalePercentChoices()); a hand-edited config.ini may hold anything in
    // [CfgMinUIScalePercent, CfgMaxUIScalePercent].
    inline constexpr int CfgDefaultUIScalePercent = 100;

    // Players' names over their heads on by default, the NPCs' off (towns would fill with guards).
    inline constexpr bool CfgDefaultOverheadPlayerNames = true;
    inline constexpr bool CfgDefaultOverheadNpcNames = false;
    inline constexpr bool CfgDefaultOverheadTitles = true;
    // On by default (ghinzuro, 2026-11-03).
    inline constexpr bool CfgDefaultTitleEffects = true;
    // On by default, a switch in Options (ghinzuro, 2026-11-03).
    inline constexpr bool CfgDefaultTitlesTownsOnly = true;

    // The loot filter's «Ценное» defaults (ghinzuro 2026-11-11), equal to LootFilter::Preset::Default
    // (test_loot_filter checks it). An empty category string means the Default preset's categories.
    inline constexpr bool CfgDefaultLootFilterEnabled = true;
    inline constexpr bool CfgDefaultLootFilterMarker = true;
    inline constexpr bool CfgDefaultLootFilterSound = false;
    inline constexpr wchar_t CfgDefaultLootFilterCategories[] = L"";
    inline constexpr int CfgDefaultLootFilterRarity = 3; // excellent + ancient
    inline constexpr int CfgDefaultLootFilterMinLevel = 9;
    inline constexpr int CfgDefaultLootFilterZenMin = 0; // off
    // At most this many names are read back, as LootFilter::MaxNames (test_loot_filter checks it).
    inline constexpr int CfgMaxLootFilterNames = 32;

    // "auto" follows the device: a phone or tablet in the browser gets the phone layout and touch.
    inline constexpr wchar_t CfgDefaultInterfaceView[] = L"auto";
    inline constexpr wchar_t CfgDefaultControls[] = L"auto";
    inline constexpr int CfgDefaultTouchButtonSize = 100;
    inline constexpr int CfgMinTouchButtonSize = 60;
    inline constexpr int CfgMaxTouchButtonSize = 150;
    // The top row starts hidden behind its toggle (ghinzuro, 2026-11-11).
    inline constexpr bool CfgDefaultTouchTopRow = false;
    // On: a pad plugged in plays. Off for a pad that Steam Input or the handheld's own software
    // already turns into a mouse and keys, which would otherwise press twice.
    inline constexpr bool CfgDefaultGamepad = true;
    inline constexpr bool CfgDefaultGamepadRumble = true;

    // Bounds GameConfig clamps to, on load and on set. Below 75% text gets too small to read;
    // above 150% a docked panel no longer fits a small window.
    inline constexpr int CfgMinUIScalePercent = 75;
    inline constexpr int CfgMaxUIScalePercent = 150;

    inline constexpr bool CfgDefaultEnableAnimationTaskPool = false;

    // Opt-in until real effects have been visually checked on target hardware.
    inline constexpr bool CfgDefaultSortParticleDraws = false;
    inline constexpr bool CfgDefaultVSync = true;

    // The look a missing or invalid [Render] Look stands for.
    inline constexpr wchar_t CfgDefaultLook[] = L"Classic";
    // 8x anisotropy on textures with mip levels.
    inline constexpr int CfgDefaultAnisotropy = 8;

    // The samples the Enhanced look renders with (Classic renders without); a
    // requested count is lowered to what the GPU supports.
    inline constexpr int CfgDefaultAntialiasing = 4;
    inline constexpr bool CfgDefaultEnhancedLighting = true;
    inline constexpr bool CfgDefaultDynamicLights = true;
    inline constexpr bool CfgDefaultLightMood = true;
    inline constexpr bool CfgDefaultPostProcessing = true;

    // A missing or invalid [Render] GraphicsPreset: chosen from the GPU at start-up.
    inline constexpr wchar_t CfgDefaultGraphicsPreset[] = L"Auto";
    inline constexpr bool CfgDefaultFxaa = false;
    inline constexpr bool CfgDefaultHdTextures = false;
    // 0 = by the machine at every start, 1 = full, 2 = half (Render/Textures/HdTextureSize.h).
    inline constexpr int CfgDefaultHdTextureSize = 0;
    inline constexpr int CfgMinHdTextureSize = 0;
    inline constexpr int CfgMaxHdTextureSize = 2;
    inline constexpr int CfgDefaultSmoothModels = 1;
    inline constexpr bool CfgDefaultNormalMaps = true;
    inline constexpr int CfgDefaultTerrainDetail = 1;
    inline constexpr int CfgMinTerrainDetail = 0;
    inline constexpr int CfgMaxTerrainDetail = 2;
    inline constexpr int CfgDefaultTerrainDetailStrength = 60;
    inline constexpr int CfgMinTerrainDetailStrength = 0;
    inline constexpr int CfgMaxTerrainDetailStrength = 100;
    inline constexpr int CfgDefaultTerrainVariation = 2;
    inline constexpr int CfgMinTerrainVariation = 0;
    inline constexpr int CfgMaxTerrainVariation = 2;
    inline constexpr int CfgDefaultTerrainVariationStrength = 100;
    inline constexpr int CfgMinTerrainVariationStrength = 0;
    inline constexpr int CfgMaxTerrainVariationStrength = 100;
    inline constexpr int CfgDefaultTerrainWater = 1;
    inline constexpr int CfgMinTerrainWater = 0;
    inline constexpr int CfgMaxTerrainWater = 1;
    inline constexpr int CfgDefaultTerrainWaterStrength = 100;
    inline constexpr int CfgMinTerrainWaterStrength = 0;
    inline constexpr int CfgMaxTerrainWaterStrength = 100;
    inline constexpr int CfgDefaultSsao = 1;
    inline constexpr int CfgMinSsao = 0;
    inline constexpr int CfgMaxSsao = 1;
    inline constexpr int CfgDefaultSsaoStrength = 100;
    inline constexpr int CfgMinSsaoStrength = 0;
    inline constexpr int CfgMaxSsaoStrength = 100;
    inline constexpr int CfgDefaultWind = 1;
    inline constexpr int CfgMinWind = 0;
    inline constexpr int CfgMaxWind = 1;
    inline constexpr int CfgDefaultWindStrength = 100;
    inline constexpr int CfgMinWindStrength = 0;
    inline constexpr int CfgMaxWindStrength = 100;
    inline constexpr int CfgDefaultSunShadows = 0;
    inline constexpr int CfgMinSunShadows = 0;
    inline constexpr int CfgMaxSunShadows = 2;
    inline constexpr int CfgDefaultSharpen = 0;
    inline constexpr int CfgMinSharpen = 0;
    inline constexpr int CfgMaxSharpen = 100;
    // The share of the screen's usable area a new installation's window takes when no display mode fits.
    inline constexpr int CfgFallbackWindowPercent = 90;

    inline constexpr bool CfgDefaultDisableEffects = false;
    inline constexpr bool CfgDefaultDisableParticles = false;
    inline constexpr bool CfgDefaultDisableSkillEffectModels = false;
    inline constexpr bool CfgDefaultDisableBoids = false;
    inline constexpr bool CfgDefaultDisableWingShadow = false;
    inline constexpr bool CfgDefaultDisableBodyShadows = false;
    inline constexpr bool CfgDefaultSoftShadows = true;
    inline constexpr int CfgDefaultFpsCap = -1;  // Uncapped
    inline constexpr int CfgMinEffectLevel = 0;
    inline constexpr int CfgMaxEffectLevel = 4;
    inline constexpr int CfgDefaultEffectLevel = CfgMaxEffectLevel;
    inline constexpr bool CfgDefaultFullEffects = true;

    // "default" = this app's own platform-aware pick (prefers Vulkan on Windows to avoid
    // D3D12's vsync-cap bug; SDL's own auto-pick elsewhere). Other accepted values:
    // "vulkan", "direct3d12" (alias "d3d12"), "metal" -- forces that SDL_gpu driver on any
    // platform, falling back to auto-pick with a warning log if it's unavailable.
    inline constexpr wchar_t CfgDefaultRenderBackend[] = L"default";
}
