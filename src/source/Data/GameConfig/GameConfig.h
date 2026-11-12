#pragma once

#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include "Core/Platform/WinCompat.h"
#include "Data/GameConfig/GameConfigValidation.h"

class GameConfig
{
public:
    static GameConfig& GetInstance();

    void Load();
    void Save();

    // config.ini beside the executable. Its [EnhancedTuning] section is read and written on its own
    // (Data/GameConfig/PersonalTuning.h); Save() leaves it as it is.
    const std::filesystem::path& GetConfigPath() const
    {
        return m_configPath;
    }

    // Window
    int  GetWindowWidth()  const { return m_windowWidth; }
    int  GetWindowHeight() const { return m_windowHeight; }
    bool GetWindowMode()   const { return m_windowMode; }

    void SetWindowSize(int width, int height);
    void SetWindowMode(bool windowed);

    // Audio — volume 0 = off, >0 = on. No separate Enabled flag.
    int  GetSoundVolume()  const { return m_soundVolume; }
    int  GetMusicVolume()  const { return m_musicVolume; }

    void SetSoundVolume(int level);
    void SetMusicVolume(int level);

    // Login
    bool GetRememberMe() const { return m_rememberMe; }
    void SetRememberMe(bool remember);

    // Whether the password (not just the username) may be persisted. Off by
    // default: "remember me" saves only the username unless the player opts in
    // on a machine they trust.
    bool GetSavePassword() const { return m_savePassword; }
    void SetSavePassword(bool save);

    // Drops the saved username and password from config.ini and revokes the
    // save-password consent. Used when the player edits the credentials.
    void ClearCredentials();

    std::wstring GetLanguageSelection() const { return m_languageSelection; }
    void SetLanguageSelection(const std::wstring& lang);

    void SetEncryptedUsername(const std::wstring& encryptedUsername);
    std::wstring GetEncryptedUsername() const { return m_encryptedUsername; }

    void SetEncryptedPassword(const std::wstring& encryptedPassword);
    std::wstring GetEncryptedPassword() const { return m_encryptedPassword; }

    // Connection
    std::wstring GetServerIP() const { return m_serverIP; }
    int GetServerPort() const { return m_serverPort; }

    void SetServerIP(const std::wstring& ip);
    void SetServerPort(int port);

    // UI — I18N locale code ("en", "de", ...) for the typed translation
    // accessors. Distinct from GetLanguageSelection above, which is the
    // legacy "Eng"/"Por"/"Spn" data-dir prefix used by .bmd asset loaders.
    std::wstring GetUILocale() const { return m_uiLocale; }
    void SetUILocale(const std::wstring& locale);

    // UI font family name (GDI face name). Empty = platform default.
    std::wstring GetFontSelection() const { return m_fontSelection; }
    void SetFontSelection(const std::wstring& font);

    // Active RmlUi theme name ("legacy"/"modern") -- see UI::RmlBridge::RmlTheme.
    std::wstring GetRmlTheme() const { return m_rmlTheme; }
    void SetRmlTheme(const std::wstring& theme);

    // Global RmlUi UI scale, as a percentage (100 = normal). Drives RmlUiRuntime's
    // Context::SetDensityIndependentPixelRatio() call; only RCSS authored in `dp` responds. The
    // setter clamps to [CfgDefaults::CfgMinUIScalePercent, CfgDefaults::CfgMaxUIScalePercent];
    // a value read from disk is taken as-is, so a hand-edited config.ini stays authoritative
    // until something writes the setting back.
    int GetUIScalePercent() const { return m_uiScalePercent; }
    void SetUIScalePercent(int percent);

    // Names over the heads of players and of NPCs (UI::OverheadNames).
    bool GetOverheadPlayerNames() const { return m_overheadPlayerNames; }
    void SetOverheadPlayerNames(bool shown) { m_overheadPlayerNames = shown; }
    bool GetOverheadNpcNames() const { return m_overheadNpcNames; }
    void SetOverheadNpcNames(bool shown) { m_overheadNpcNames = shown; }
    // Title plates over the players (UI::TitlePlates).
    bool GetOverheadTitles() const { return m_overheadTitles; }
    void SetOverheadTitles(bool shown) { m_overheadTitles = shown; }
    // The title plates' effects (UI::TitlePlates::Effects).
    bool GetTitleEffects() const { return m_titleEffects; }
    void SetTitleEffects(bool shown) { m_titleEffects = shown; }
    bool GetBackgroundSound() const { return m_backgroundSound; }
    void SetBackgroundSound(bool on) { m_backgroundSound = on; }
    bool GetTitlesTownsOnly() const { return m_titlesTownsOnly; }
    void SetTitlesTownsOnly(bool townsOnly) { m_titlesTownsOnly = townsOnly; }

    // The loot highlight filter's raw settings; GameLogic::Items::LootFilterRuntime turns them into rules.
    bool GetLootFilterEnabled() const { return m_lootFilterEnabled; }
    void SetLootFilterEnabled(bool on) { m_lootFilterEnabled = on; }
    bool GetLootFilterMarker() const { return m_lootFilterMarker; }
    void SetLootFilterMarker(bool on) { m_lootFilterMarker = on; }
    bool GetLootFilterSound() const { return m_lootFilterSound; }
    void SetLootFilterSound(bool on) { m_lootFilterSound = on; }
    const std::wstring& GetLootFilterCategories() const { return m_lootFilterCategories; }
    void SetLootFilterCategories(const std::wstring& digits) { m_lootFilterCategories = digits; }
    int GetLootFilterRarity() const { return m_lootFilterRarity; }
    void SetLootFilterRarity(int mask) { m_lootFilterRarity = mask; }
    int GetLootFilterMinLevel() const { return m_lootFilterMinLevel; }
    void SetLootFilterMinLevel(int level) { m_lootFilterMinLevel = level; }
    int GetLootFilterZenMin() const { return m_lootFilterZenMin; }
    void SetLootFilterZenMin(int zen) { m_lootFilterZenMin = zen; }
    const std::vector<std::wstring>& GetLootFilterNames() const { return m_lootFilterNames; }
    void SetLootFilterNames(const std::vector<std::wstring>& names) { m_lootFilterNames = names; }

    // The phone or PC layout and the controls, as config.ini words (UI::Mobile::InterfaceMode).
    const std::wstring& GetInterfaceView() const { return m_interfaceView; }
    void SetInterfaceView(const std::wstring& view) { m_interfaceView = view; }
    const std::wstring& GetControls() const { return m_controls; }
    void SetControls(const std::wstring& controls) { m_controls = controls; }
    // The on-screen controls' size in percent (UI::Mobile::TouchUnit).
    int GetTouchButtonSize() const { return m_touchButtonSize; }
    void SetTouchButtonSize(int percent);
    // Whether the phone's top row of buttons is open (UI/HUD/TouchControls).
    bool GetTouchTopRow() const { return m_touchTopRow; }
    void SetTouchTopRow(bool open) { m_touchTopRow = open; }
    // Whether the game reads a gamepad (Core::Input::Gamepad::SetEnabled).
    bool GetGamepad() const { return m_gamepad; }
    void SetGamepad(bool enabled) { m_gamepad = enabled; }
    // Whether the pad vibrates on hits and on death (Core::Input::Gamepad::SetRumble).
    bool GetGamepadRumble() const { return m_gamepadRumble; }
    void SetGamepadRumble(bool on) { m_gamepadRumble = on; }

    // Chat commands - the favourites and the named templates of the command
    // window. They belong to the installation, not to a character.
    // A template is stored as "name|command|value|value|...".
    const std::vector<std::wstring>& GetChatCommandFavourites() const
    {
        return m_chatCommandFavourites;
    }
    void SetChatCommandFavourites(const std::vector<std::wstring>& favourites);

    const std::vector<std::wstring>& GetChatCommandTemplates() const
    {
        return m_chatCommandTemplates;
    }
    void SetChatCommandTemplates(const std::vector<std::wstring>& templates);

    // The game master panel's saved items (its Items tab), as "group|number|level|...|name"
    // (GmPanelWindow's ItemDraft). They belong to the installation, not to a character.
    const std::vector<std::wstring>& GetGmItemFavourites() const { return m_gmItemFavourites; }
    void SetGmItemFavourites(const std::vector<std::wstring>& favourites);

    // Camera
    int GetZoom() const { return m_zoom; }
    void SetZoom(int zoom);
    // What F9, F10 and the wheel left, for the next start and the next entry into the game.
    // 0 = the default camera, 1 = the orbital one (CameraMode).
    int GetCameraMode() const { return m_cameraMode; }
    void SetCameraMode(int mode) { m_cameraMode = mode; }
    bool GetZoomLock() const { return m_zoomLock; }
    void SetZoomLock(bool locked) { m_zoomLock = locked; }
    // The default camera's step on its zoom ladder (DefaultCamera).
    int GetZoomLevel() const { return m_zoomLevel; }
    void SetZoomLevel(int level) { m_zoomLevel = level; }
    // The orbital camera's turn and tilt from the default view, in whole degrees.
    int GetOrbitYaw() const { return m_orbitYaw; }
    int GetOrbitPitch() const { return m_orbitPitch; }
    void SetOrbit(int yaw, int pitch)
    {
        m_orbitYaw = yaw;
        m_orbitPitch = pitch;
    }
    // Writes the [Camera] section alone: cheap enough for every wheel tick. In the browser
    // index.html copies config.ini into the page's storage a few seconds later.
    void SaveCamera();

    // Render
    bool GetSortParticleDraws() const { return m_sortParticleDraws; }
    bool GetVSyncEnabled() const { return m_vsyncEnabled; }
    void SetVSyncEnabled(bool enabled);
    // The look preset ([Render] Look).
    LookPreset GetLook() const
    {
        return m_look;
    }
    void SetLook(LookPreset look);
    int GetAnisotropy() const
    {
        return m_anisotropy;
    }
    void SetAnisotropy(int anisotropy);
    // Samples per pixel: 0 (off), 2, 4 or 8.
    int GetAntialiasing() const
    {
        return m_antialiasing;
    }
    void SetAntialiasing(int samples);
    // [Render] GraphicsPreset, Fxaa (on/off) and Sharpen (0 to 100 percent).
    GraphicsPreset GetGraphicsPreset() const { return m_graphicsPreset; }
    // Whether config.ini already had a [Render] Look key when it was read: false on a new install only.
    bool HadSavedLook() const { return m_hadSavedLook; }
    void SetGraphicsPreset(GraphicsPreset preset) { m_graphicsPreset = preset; }
    bool GetFxaa() const { return m_fxaa; }
    void SetFxaa(bool enabled) { m_fxaa = enabled; }
    // [Render] HdTextures: load the upscaled copies from Data/HD where there is one (after a restart).
    bool GetHdTextures() const { return m_hdTextures; }
    void SetHdTextures(bool enabled) { m_hdTextures = enabled; }
    // [Render] HdTextureSize: how large the HD textures load, 0 by the machine, 1 full, 2 half
    // (clamped; after a restart; Render/Textures/HdTextureSize.h).
    int GetHdTextureSize() const { return m_hdTextureSize; }
    void SetHdTextureSize(int size);
    // [Render] SmoothModels: with HD textures on, how many times each model triangle is split when
    // it loads (0 = off, 1, 2; Render/Models/ModelSmoothing.h).
    int GetSmoothModels() const { return m_smoothModels; }
    // [Render] NormalMaps: with HD textures on, model textures get a relief map for the Enhanced
    // lighting (Render/Textures/NormalMaps.h).
    bool GetNormalMaps() const { return m_normalMaps; }
    // [Render] TerrainDetail: with HD textures on, grain on the ground near the camera, 0 off,
    // 1 self-detail, 2 noise; TerrainDetailStrength in percent (both clamped; after a restart).
    int GetTerrainDetail() const { return m_terrainDetail; }
    int GetTerrainDetailStrength() const { return m_terrainDetailStrength; }
    // [Render] TerrainVariation: with HD textures on, breaks the ground textures' repeat, 0 off,
    // 1 macro colour, 2 macro colour + mirrored blocks (Render/Terrain/TerrainVariation.h);
    // TerrainVariationStrength in percent (both clamped; after a restart).
    int GetTerrainVariation() const { return m_terrainVariation; }
    int GetTerrainVariationStrength() const { return m_terrainVariationStrength; }
    // [Render] SunShadows: 0 off, 1 medium, 2 high (clamped).
    int GetSunShadows() const { return m_sunShadows; }
    void SetSunShadows(int level);
    int GetSharpen() const { return m_sharpen; }
    void SetSharpen(int percent);
    bool GetEnhancedLighting() const { return m_enhancedLighting; }
    void SetEnhancedLighting(bool enabled) { m_enhancedLighting = enabled; }
    bool GetDynamicLights() const { return m_dynamicLights; }
    void SetDynamicLights(bool enabled) { m_dynamicLights = enabled; }
    bool GetLightMood() const { return m_lightMood; }
    void SetLightMood(bool enabled) { m_lightMood = enabled; }
    bool GetPostProcessing() const { return m_postProcessing; }
    void SetPostProcessing(bool enabled) { m_postProcessing = enabled; }

    // DXP-23's per-system effect-cost toggles, promoted from console-only ($effects ...)
    // diagnostics to persisted, in-game-options-exposed settings -- see MainScene.h's
    // SetDisableEffects/SetDisableParticles/SetDisableSkillEffectModels/SetDisableBoids/
    // SetDisableWingShadow for what each one actually gates.
    bool GetDisableEffects() const { return m_disableEffects; }
    void SetDisableEffects(bool disabled);
    bool GetDisableParticles() const { return m_disableParticles; }
    void SetDisableParticles(bool disabled);
    bool GetDisableSkillEffectModels() const { return m_disableSkillEffectModels; }
    void SetDisableSkillEffectModels(bool disabled);
    bool GetDisableBoids() const { return m_disableBoids; }
    void SetDisableBoids(bool disabled);
    bool GetDisableWingShadow() const { return m_disableWingShadow; }
    void SetDisableWingShadow(bool disabled);
    bool GetDisableBodyShadows() const { return m_disableBodyShadows; }
    void SetDisableBodyShadows(bool disabled);
    // [Render] SoftShadows: soft body shadows; only the browser build draws them.
    bool GetSoftShadows() const { return m_softShadows; }
    void SetSoftShadows(bool enabled) { m_softShadows = enabled; }

    // User-chosen FPS cap when VSync is off; -1 = uncapped. Same promotion story as the toggles
    // above, this time for the console-only `$fps <N>` diagnostic (SceneManager::SetTargetFps).
    int GetFpsCap() const { return m_fpsCap; }
    void SetFpsCap(int fps);

    // [Render] EffectLevel (0..4, 4 = everything) and FullEffects: Options' effect bar and its
    // "Render full effects" checkbox.
    int GetEffectLevel() const { return m_effectLevel; }
    void SetEffectLevel(int level);
    bool GetFullEffects() const { return m_fullEffects; }
    void SetFullEffects(bool enabled) { m_fullEffects = enabled; }

    // GPU backend override -- "default"/"vulkan"/"direct3d12"/"metal". Manual config.ini edit
    // only, same as GetSortParticleDraws() above: no setter, no in-game options-window exposure.
    std::wstring GetRenderBackend() const { return m_renderBackend; }

    // [UI] EnableAnimationTaskPool: manual config.ini edit only, like the two above.
    bool GetEnableAnimationTaskPool() const { return m_enableAnimationTaskPool; }

    // Helpers
    static std::wstring BinaryToHex(const BYTE* data, DWORD size);
    static std::vector<BYTE> HexToBinary(const std::wstring& hex);

    void DecryptCredentials(wchar_t* outUser, wchar_t* outPass, size_t userBufSize, size_t passBufSize);
    void EncryptAndSaveCredentials(const wchar_t* user, const wchar_t* pass);

private:
    GameConfig();
    GameConfig(const GameConfig&) = delete;
    GameConfig& operator=(const GameConfig&) = delete;

    std::filesystem::path m_configPath;

    int  m_windowWidth;
    int  m_windowHeight;
    bool m_windowMode;

    int  m_soundVolume;
    int  m_musicVolume;

    std::vector<std::wstring> m_chatCommandFavourites;
    std::vector<std::wstring> m_chatCommandTemplates;
    std::vector<std::wstring> m_gmItemFavourites;

    bool m_rememberMe;
    bool m_savePassword;
    std::wstring m_languageSelection;
    std::wstring m_encryptedUsername;
    std::wstring m_encryptedPassword;

    std::wstring m_serverIP;
    int m_serverPort;

    std::wstring m_uiLocale;
    std::wstring m_fontSelection;
    std::wstring m_rmlTheme;
    int m_uiScalePercent;
    bool m_overheadPlayerNames = true;
    bool m_overheadNpcNames = false;
    bool m_overheadTitles = true;
    bool m_titleEffects = true;
    bool m_titlesTownsOnly = true;
    bool m_lootFilterEnabled = true;
    bool m_lootFilterMarker = true;
    bool m_lootFilterSound = false;
    std::wstring m_lootFilterCategories;
    int m_lootFilterRarity = 3;
    int m_lootFilterMinLevel = 9;
    int m_lootFilterZenMin = 0;
    std::vector<std::wstring> m_lootFilterNames;
    bool m_backgroundSound = true;
    std::wstring m_interfaceView;
    std::wstring m_controls;
    int m_touchButtonSize = 100;
    bool m_touchTopRow = false;
    bool m_gamepad = true;
    bool m_gamepadRumble = true;
    bool m_enableAnimationTaskPool;

    int m_zoom;
    int m_cameraMode = 0;
    bool m_zoomLock = true;
    int m_zoomLevel = 3;
    int m_orbitYaw = 0;
    int m_orbitPitch = 0;
    bool m_sortParticleDraws;
    bool m_vsyncEnabled;
    LookPreset m_look;
    int m_anisotropy;
    int m_antialiasing;
    bool m_enhancedLighting;
    bool m_dynamicLights;
    bool m_lightMood;
    bool m_postProcessing;
    GraphicsPreset m_graphicsPreset = GraphicsPreset::Auto;
    bool m_hadSavedLook = false;
    bool m_fxaa = false;
    bool m_hdTextures = false;
    int m_hdTextureSize = 0;
    int m_smoothModels = 1;
    bool m_normalMaps = true;
    int m_terrainDetail = 1;
    int m_terrainDetailStrength = 60;
    int m_terrainVariation = 2;
    int m_terrainVariationStrength = 100;
    int m_sunShadows = 0;
    int m_sharpen = 0;
    std::wstring m_renderBackend;
    bool m_disableEffects;
    bool m_disableParticles;
    bool m_disableSkillEffectModels;
    bool m_disableBoids;
    bool m_disableWingShadow;
    bool m_disableBodyShadows;
    bool m_softShadows = true;
    int m_fpsCap;
    int m_effectLevel = 4;
    bool m_fullEffects = true;

    int ReadInt(const wchar_t* section, const wchar_t* key, int defaultValue) const;
    void WriteInt(const wchar_t* section, const wchar_t* key, int value);

    bool ReadBool(const wchar_t* section, const wchar_t* key, bool defaultValue) const;
    void WriteBool(const wchar_t* section, const wchar_t* key, bool value);

    // Reads at most maxCount entries, whatever <prefix>Count says.
    std::vector<std::wstring> ReadStringList(const wchar_t* section, const wchar_t* keyPrefix,
                                             int maxCount = (std::numeric_limits<int>::max)());
    void LoadLootFilter();
    void SaveLootFilter();
    void WriteStringList(const wchar_t* section, const wchar_t* keyPrefix, const std::vector<std::wstring>& values);
    std::wstring ReadString(const wchar_t* section, const wchar_t* key, const std::wstring& defaultValue);
    void WriteString(const wchar_t* section, const wchar_t* key, const std::wstring& value);

    void RemoveObsoleteKey(const wchar_t* section, const wchar_t* key);
    void RemoveObsoleteSection(const wchar_t* section);

    std::wstring DecryptSetting(const std::wstring& hexInput);
    std::wstring EncryptSetting(const wchar_t* input);
};

// The [Render] Look of a config.ini as GameConfig::Load reads it.
LookPreset ReadLookPreset(const std::filesystem::path& configFile);
bool HasSavedLook(const std::filesystem::path& configFile);
