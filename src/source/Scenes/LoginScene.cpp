///////////////////////////////////////////////////////////////////////////////
// LoginScene.cpp - Login scene implementation
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "LoginScene.h"
#include "Camera/CameraUtility.h"
#include "Camera/CameraManager.h"
#include "Camera/CameraMove.h"
#include "Audio/DSPlaySound.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Renderer/MuRenderer.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Effects/ZzzEffect.h"
#include "Engine/AI/GOBoid.h"
#include "GameLogic/Pets/w_PetProcess.h"
#include "World/MapInfra/MapManager.h"
#include "UI/Core/SceneUICoordinator.h"
#include "UI/Windows/CreditWin.h"
#include "Core/Input/Input.h"
#include "Network/Server/WSclient.h"
#include "Core/Utilities/Log/muConsoleDebug.h"
#include "I18N/All.h"
#include "Engine/Object/ZzzCharacter.h"
#include "SceneCommon.h"
#include "Core/Utilities/FrameProfiler.h"
#include "Engine/Object/ZzzOpenData.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/CommonMessageBox.h"
#include "UI/Scaling/UITransform.h"
#include "LoginSceneOverlay.h"
#include "Render/Text/CUIRenderText.h"
#include "Scenes/SceneLightingSetup.h"
#include "Scenes/ScenePostProcessing.h"
#include "Render/Sprites/GlobalBitmap.h"

#include <cmath>

// External declarations
extern int DeleteGuildIndex;
extern EGameScene SceneFlag;
extern int g_iChatInputType;
extern float g_fMULogoAlpha;
extern wchar_t m_Username[MAX_USERNAME_SIZE + 1];
extern CHARACTER* Hero;
extern double WorldTime;
extern HFONT g_hFont;
extern wchar_t m_ExeVersion[11];
extern HWND g_hWnd;
extern unsigned int WindowWidth;
extern unsigned int WindowHeight;

#ifdef _EDITOR
extern "C" float DevEditor_GetLoginTerrainDist();
#endif

//=============================================================================
// LoginScene Camera State (local to this file)
//=============================================================================
namespace {

struct LoginCameraState {
    int walkCut = 0;
    int currentCount = -1;
    int currentNumber = 0;
    int currentWalkType = 0;
    vec3_t currentPosition = {0, 0, 0};
    vec3_t currentAngle = {0, 0, 0};
    float currentWalkDelta[6] = {0, 0, 0, 0, 0, 0};

    // Predefined camera animation paths (6 paths × 6 values each)
    static constexpr float WALK_PATHS[6][6] = {
        {0.f, -1000.f, 500.f, -80.f, 0.f, 0.f},
        {0.f, -1100.f, 500.f, -80.f, 0.f, 0.f},
        {0.f, -1100.f, 500.f, -80.f, 0.f, 0.f},
        {0.f, -1100.f, 500.f, -80.f, 0.f, 0.f},
        {0.f, -1100.f, 500.f, -80.f, 0.f, 0.f},
        {200.f, -800.f, 250.f, -87.f, 0.f, -10.f}
    };

    void Reset() {
        currentCount = -1;
        walkCut = 0;
        currentNumber = 0;
        currentWalkType = 0;
    }
};

// File-local camera state instance
LoginCameraState g_loginCamera;

}  // namespace

//=============================================================================
// Skynet backdrop scene (ghinzuro 2026-11-08, option B)
//=============================================================================
// The castle of map 73 is gone from the login screen: a painting of the sky city Skynet
// (Data/Logo/SkynetLogin.OZJ) fills the screen, and a knight in the top set with wings stands in
// front of it, seen by a camera that doesn't move. The map's terrain still loads (the hero stands
// on its heights) but isn't drawn, and its objects aren't loaded at all (MapManager::LoadWorld).
namespace LoginBackdrop
{
constexpr int TEXTURE = BITMAP_LOG_IN + 18;
constexpr float IMAGE_ASPECT = 2.0f;  // 2048x1024
// Which part of the picture stays on screen when the window is narrower (0 left, 1 right) or
// wider (0 top, 1 bottom) than the picture.
constexpr float ANCHOR_X = 0.45f;
constexpr float ANCHOR_Y = 0.5f;

// Not the hero's slot 0: the game moves the hero by the mouse.
constexpr int HERO_INDEX = 1;
constexpr float HERO_X = 128.f * TERRAIN_SCALE;
constexpr float HERO_Y = 128.f * TERRAIN_SCALE;
constexpr float HERO_ANGLE = -65.f; // 0 faces the camera, more turns the face to the screen's right
// The knight rides the seasonal Fenrir; false shows the Fenrir alone.
constexpr bool SHOW_RIDER = false;

// The camera relative to the hero's feet; it looks along +Y, so +X is the screen's right. Its X
// follows the window's shape, so the hero stands at the same place of the screen: HERO_SCREEN_X
// of the half width right of the centre (the picture's empty dark side).
constexpr float HERO_SCREEN_X = 0.55f;
constexpr float CAMERA_Y = -720.f;
constexpr float CAMERA_Z = 170.f;
constexpr float CAMERA_PITCH = -90.f;  // -90 looks straight ahead
constexpr float CAMERA_FOV = 45.f;

vec3_t g_heroFeet = {HERO_X, HERO_Y, 0.f};

void CreateHeroOnStage()
{
    CHARACTER* c = CreateHero(HERO_INDEX, CLASS_KNIGHT, 0, HERO_X, HERO_Y, HERO_ANGLE);
    // The Season 4 knight set at +15 with excellent shine, the third wings and the top sword.
    const int set[] = {MODEL_TITAN_HELM, MODEL_TITAN_ARMOR, MODEL_TITAN_PANTS, MODEL_TITAN_GLOVES, MODEL_TITAN_BOOTS};
    const int parts[] = {BODYPART_HELM, BODYPART_ARMOR, BODYPART_PANTS, BODYPART_GLOVES, BODYPART_BOOTS};
    for (int i = 0; i < 5; ++i)
    {
        c->BodyPart[parts[i]].Type = set[i];
        c->BodyPart[parts[i]].Level = 15;
        c->BodyPart[parts[i]].ExcellentFlags = 0x3F;
        c->BodyPart[parts[i]].AncientDiscriminator = 0;
    }
    c->Weapon[0].Type = MODEL_IMPERIAL_SWORD;
    c->Weapon[0].Level = 15;
    c->Weapon[0].ExcellentFlags = 0x3F;
    c->Weapon[1].Type = -1;
    c->Wing.Type = MODEL_WING_OF_STORM;
    c->Wing.Level = 15;
    c->ID[0] = L'\0';

    g_heroFeet[2] = RequestTerrainHeight(HERO_X, HERO_Y);
    c->Object.Position[2] = g_heroFeet[2];

    // The battle pass' seasonal Fenrir (the gold horn), in the current season's look (Data/Seasons.json).
    c->Helper.Type = MODEL_HORN_OF_FENRIR;
    c->Helper.Level = 0;
    c->Helper.ExcellentFlags = 0x04;
    // In a safe zone the rider walks and the Fenrir hides, so the stage is not one.
    TerrainWall[TERRAIN_INDEX_REPEAT(static_cast<int>(HERO_X / TERRAIN_SCALE), static_cast<int>(HERO_Y / TERRAIN_SCALE))] &= ~TW_SAFEZONE;
    c->SafeZone = false;
    CreateMount(MODEL_FENRIR_GOLD, c->Object.Position, &c->Object);
    SetPlayerStop(c);
}

void SetCamera()
{
    g_Camera.FOV = CAMERA_FOV;
    const float aspect = (WindowHeight > 0) ? static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight) : 4.f / 3.f;
    const float halfHeight = std::tan(CAMERA_FOV * 0.5f * 3.14159265f / 180.f) * -CAMERA_Y;
    vec3_t offset = {-HERO_SCREEN_X * halfHeight * aspect, CAMERA_Y, CAMERA_Z};
    vec3_t angle = {CAMERA_PITCH, 0.f, 0.f};
    MoveCharacterCamera(g_heroFeet, offset, angle);
}

// The picture over the whole window, cut to the window's shape (it fills it, no bars).
void RenderPicture()
{
    if (Bitmaps.GetTexture(TEXTURE) == nullptr)
        return;
    const float width = static_cast<float>(WindowWidth);
    const float height = static_cast<float>(WindowHeight);
    if (width <= 0.f || height <= 0.f)
        return;

    float u = 0.f, v = 0.f, uWidth = 1.f, vHeight = 1.f;
    const float aspect = width / height;
    if (aspect < IMAGE_ASPECT)
    {
        uWidth = aspect / IMAGE_ASPECT;
        u = (1.f - uWidth) * ANCHOR_X;
    }
    else
    {
        vHeight = IMAGE_ASPECT / aspect;
        v = (1.f - vHeight) * ANCHOR_Y;
    }

    BeginBitmap();
    DisableAlphaBlend();
    RenderBitmap(TEXTURE, 0.f, 0.f, width, height, u, v, uWidth, vHeight, false, false);
    EndBitmap();
}
}  // namespace LoginBackdrop

//=============================================================================
// Accessor functions for external use
//=============================================================================

int GetLoginCameraCount() {
    return g_loginCamera.currentCount;
}

int GetLoginCameraWalkCut() {
    return g_loginCamera.walkCut;
}

//=============================================================================
// LoginScene Implementation
//=============================================================================

void MoveCharacterCamera(vec3_t Origin, vec3_t Position, vec3_t Angle)
{
    vec3_t TransformPosition;
    g_Camera.Angle[0] = 0.f;
    g_Camera.Angle[1] = 0.f;
    g_Camera.Angle[2] = Angle[2];
    float Matrix[3][4];
    AngleMatrix(g_Camera.Angle, Matrix);
    VectorIRotate(Position, Matrix, TransformPosition);
    VectorAdd(Origin, TransformPosition, g_Camera.Position);
    g_Camera.Angle[0] = Angle[0];
}

/**
 * @brief Initializes login camera to starting position and angle.
 */
static void InitializeLoginCamera()
{
    for (int i = 0; i < 3; i++)
    {
        g_loginCamera.currentPosition[i] = LoginCameraState::WALK_PATHS[0][i];
        g_loginCamera.currentAngle[i] = LoginCameraState::WALK_PATHS[0][i + 3];
    }
    g_loginCamera.currentNumber = 1;
    g_loginCamera.currentWalkType = 1;

    for (int i = 0; i < 3; i++)
    {
        g_loginCamera.currentWalkDelta[i] = (LoginCameraState::WALK_PATHS[g_loginCamera.currentNumber][i] - g_loginCamera.currentPosition[i]) / 128;
        g_loginCamera.currentWalkDelta[i + 3] = (LoginCameraState::WALK_PATHS[g_loginCamera.currentNumber][i + 3] - g_loginCamera.currentAngle[i]) / 128;
    }
}

/**
 * @brief Calculates movement delta for transitioning to target waypoint.
 */
static void CalculateWalkDelta()
{
    for (int i = 0; i < 3; i++)
    {
        g_loginCamera.currentWalkDelta[i] = (LoginCameraState::WALK_PATHS[g_loginCamera.currentNumber][i] - g_loginCamera.currentPosition[i]) / 128;
        g_loginCamera.currentWalkDelta[i + 3] = (LoginCameraState::WALK_PATHS[g_loginCamera.currentNumber][i + 3] - g_loginCamera.currentAngle[i]) / 128;
    }
}

/**
 * @brief Selects next camera waypoint based on scene state.
 */
static void SelectNextWaypoint()
{
    if (SceneFlag == LOG_IN_SCENE)
    {
        g_loginCamera.currentNumber = rand() % 4 + 1;
        g_loginCamera.currentWalkType = rand() % 2;
    }
    else
    {
        g_loginCamera.currentNumber = 5;
        g_loginCamera.currentWalkType = 0;
    }
}

/**
 * @brief Updates camera waypoint transition timing and selection.
 */
static void UpdateCameraWaypoint()
{
    g_loginCamera.currentCount++;

    bool shouldTransition = (g_loginCamera.walkCut == 0 && g_loginCamera.currentCount >= 40) ||
                           (g_loginCamera.walkCut > 0 && g_loginCamera.currentCount >= 128);

    if (shouldTransition)
    {
        g_loginCamera.currentCount = 0;

        if (g_loginCamera.walkCut == 0)
        {
            g_loginCamera.walkCut = 1;
        }
        else
        {
            SelectNextWaypoint();
        }

        CalculateWalkDelta();
    }
}

/**
 * @brief Interpolates camera position/angle towards the current target waypoint.
 *
 * Two walk modes, selected per waypoint via WALK_PATHS[...][6]:
 *  - Type 0 (smooth): exponential ease toward target (divide by SMOOTH_EASING_FACTOR each frame).
 *  - Type 1 (linear): step by a precomputed per-frame delta (see CalculateWalkDelta).
 *
 * Both modes update all 3 position components AND all 3 angle components so camera
 * rotation tracks the waypoint pose, not just X/Y translation.
 */
static void InterpolateCameraMovement()
{
    constexpr int SMOOTH_EASING_FACTOR = 6;
    const int target = g_loginCamera.currentNumber;

    if (g_loginCamera.currentWalkType == 0)
    {
        for (int i = 0; i < 3; i++)
        {
            g_loginCamera.currentPosition[i] += (LoginCameraState::WALK_PATHS[target][i] - g_loginCamera.currentPosition[i]) / SMOOTH_EASING_FACTOR;
            g_loginCamera.currentAngle[i]    += (LoginCameraState::WALK_PATHS[target][i + 3] - g_loginCamera.currentAngle[i]) / SMOOTH_EASING_FACTOR;
        }
    }
    else
    {
        for (int i = 0; i < 3; i++)
        {
            g_loginCamera.currentPosition[i] += g_loginCamera.currentWalkDelta[i];
            g_loginCamera.currentAngle[i]    += g_loginCamera.currentWalkDelta[i + 3];
        }
    }
}

/**
 * @brief Updates login scene camera animation along predefined waypoint path.
 */
void MoveCamera()
{
    if (CCameraMove::GetInstancePtr()->IsTourMode())
    {
        return;
    }

    if (SceneFlag == LOG_IN_SCENE)
    {
        LoginBackdrop::SetCamera();
        return;
    }

    if (g_loginCamera.currentCount == -1)
    {
        InitializeLoginCamera();
    }

    UpdateCameraWaypoint();
    InterpolateCameraMovement();

    g_Camera.FOV = 45.f;
    vec3_t Position;
    Vector(0.f, 0.f, 0.f, Position);
    MoveCharacterCamera(Position, g_loginCamera.currentPosition, g_loginCamera.currentAngle);
}

void CreateLogInScene()
{
    EnableMainRender = true;
    gMapManager.WorldActive = WD_73NEW_LOGIN_SCENE;

    gMapManager.LoadWorld(gMapManager.WorldActive);

    OpenLogoSceneData();

    CSceneUICoordinator::Instance().CreateLoginScene();

    CurrentProtocolState = REQUEST_JOIN_SERVER;
    CreateSocket(szServerIpAddress, g_ServerPort);

    GuildInputEnable = false;
    TabInputEnable = false;
    GoldInputEnable = false;
    InputEnable = true;
    ClearInput();

    if (g_iChatInputType == 0)
    {
        wcscpy_s(InputText[0], 256, m_Username);
        InputLength[0] = wcslen(InputText[0]);
        InputTextMax[0] = MAX_USERNAME_SIZE;
        if (InputLength[0] == 0)	InputIndex = 0;
        else InputIndex = 1;
    }
    InputNumber = 2;
    InputTextHide[1] = 1;

    // FIX: Enable tour mode with offset correction
    // Tour mode waypoints work well for movement, but need position offset
    // Offset is applied in CCameraMove::GetCurrentCameraPos()
    // No camera tour over the castle: the Skynet picture and a hero, seen from one spot.
    CCameraMove::GetInstancePtr()->SetTourMode(FALSE);
    LoginBackdrop::CreateHeroOnStage();
    LoginBackdrop::SetCamera();

    MoveMainCamera();

    g_fMULogoAlpha = 0;

    ::PlayMp3(MUSIC_LOGIN_THEME);

    g_ErrorReport.Write(L"> Login Scene init success.\r\n");
}

void NewMoveLogInScene()
{
    if (!InitLogIn)
    {
        InitLogIn = true;
        CreateLogInScene();
    }

    if (!g_CreditWin.IsVisible())
    {
        InitTerrainLight();
        MoveObjects();
        MoveMounts();
        MoveLeaves();

        // Update camera BEFORE checking character visibility
        MoveCamera();

        // Now check character visibility with updated frustum
        MoveCharactersClient();

        MoveEffects();
        MoveJoints();
        MoveParticles();
        MoveBoids();
        ThePetProcess().UpdatePets();
    }

    // ESC menu toggle is handled by CSceneUICoordinator::Update()
    if (RECEIVE_LOG_IN_SUCCESS == CurrentProtocolState)
    {
        g_ErrorReport.Write(L"> Request Character list\r\n");

        CCameraMove::GetInstancePtr()->SetTourMode(FALSE);

        // Tear down the login scene data before asking the server for the
        // account characters, otherwise a fast reply can be cleared again.
        ReleaseLogoSceneData();

        SceneFlag = CHARACTER_SCENE;
        CurrentProtocolState = REQUEST_CHARACTERS_LIST;
        SocketClient->ToGameServer()->SendRequestCharacterList(g_pMultiLanguage->GetLanguage());
    }

    g_ConsoleDebug->UpdateMainScene();
}

// The logo (tour mode) and the bottom lines, drawn natively when RmlUi is not available, in the
// text state NewRenderLogInScene() sets.
static void RenderLoginSceneLinesNative(bool tourMode, const wchar_t* version)
{
    if (tourMode)
    {
        EnableAlphaBlend();
        const BYTE glowLevel = static_cast<BYTE>(std::clamp(g_fMULogoAlpha - 0.3f, 0.f, 1.f) * 255.f);
        RenderColorBitmap(BITMAP_LOG_IN + 17, 320.0f - 128.0f * 0.8f, 25.0f, 256.0f * 0.8f, 128.0f * 0.8f, 0.f, 0.f,
                          1.f, 1.f, RGBA(glowLevel, glowLevel, glowLevel, glowLevel));
        EnableAlphaTest();
        const BYTE logoLevel = static_cast<BYTE>(std::clamp(g_fMULogoAlpha, 0.f, 1.f) * 255.f);
        RenderColorBitmap(BITMAP_LOG_IN + 16, 320.0f - 128.0f * 0.8f, 25.0f, 256.0f * 0.8f, 128.0f * 0.8f, 0.f, 0.f,
                          1.f, 1.f, RGBA(logoLevel, logoLevel, logoLevel, logoLevel));
    }

    SIZE Size;
    wchar_t Text[100];

    wcscpy_s(Text, 100, I18N::Game::CCopyright2001Webzen);
    Size = g_pRenderText->MeasureText(Text, lstrlen(Text));
    g_pRenderText->RenderText(335 - Size.cx, REFERENCE_HEIGHT - Size.cy - 1, Text);

    wcscpy_s(Text, 100, I18N::Game::AllRightsReserved);

    Size = g_pRenderText->MeasureText(Text, lstrlen(Text));
    g_pRenderText->RenderText(335, REFERENCE_HEIGHT - Size.cy - 1, Text);

    Size = g_pRenderText->MeasureText(version, lstrlen(version));
    g_pRenderText->RenderText(0, REFERENCE_HEIGHT - Size.cy - 1, version);
}

bool NewRenderLogInScene(HDC hDC)
{
    if (!InitLogIn) return false;

    FogEnable = false;

    vec3_t pos;
    VectorCopy(g_Camera.Position, pos);
    if (CCameraMove::GetInstancePtr()->IsCameraMove())
    {
        VectorCopy(g_Camera.Position, pos);
    }

    MoveMainCamera();

    // Play login music (called every frame — PlayMp3 no-ops if already playing)
    ::PlayMp3(MUSIC_LOGIN_THEME);

    mu::GetRenderer().SetClearColor(0.f, 0.f, 0.f, 1.f);

    // Set ViewFar BEFORE BeginOpengl so the projection matrix covers the full render distance
#ifdef _EDITOR
    g_Camera.ViewFar = DevEditor_GetLoginTerrainDist();
#else
    g_Camera.ViewFar = LoginSceneCameraDefaults::RENDER_TERRAIN_DIST;
#endif
    g_Camera.ViewNear = 100.f;  // Push near plane out to preserve z-buffer precision

    LoginBackdrop::RenderPicture();

    const auto viewport = UI::Scaling::FullReferenceViewport();
    BeginOpengl(viewport.x, viewport.y, viewport.width, viewport.height);
    Scenes::Lighting::SubmitSceneLighting();

    // LoginScene doesn't call CreateFrustrum (DefaultCamera tour mode angles differ from
    // legacy hardcoded values). Instead, TestFrustrum2D is bypassed for LOG_IN_SCENE and
    // we reset iteration bounds to cover the full terrain so stale bounds from other scenes
    // don't restrict the render loop.
    ResetFrustrumBoundsFullTerrain();

    if (!g_CreditWin.IsVisible())
    {
        if (LoginBackdrop::SHOW_RIDER)
        {
            FRAME_PROFILE(Characters);
            RenderCharactersClient();
        }
        { FRAME_PROFILE(Objects); RenderMount(); }
        {
            FRAME_PROFILE(Objects);
            {
                FRAME_PROFILE(Other);
                RenderObjects();
            }
        }
        { FRAME_PROFILE(Effects); RenderJoints(); }
        { FRAME_PROFILE(Effects); RenderEffects(); }
        { FRAME_PROFILE(Effects); CheckSprites(); }
        { FRAME_PROFILE(Effects); RenderLeaves(); }
        { FRAME_PROFILE(Effects); RenderBoids(); }
        {
            FRAME_PROFILE(Objects);
            {
                FRAME_PROFILE(Other);
                RenderObjects_AfterCharacter();
            }
        }
        {
            FRAME_PROFILE(Characters);
            Scenes::PostProcessing::RenderPetsWithoutBloom();
        }
    }

    BeginSprite();
    RenderSprites();
    RenderParticles();
    EndSprite();

    Scenes::PostProcessing::EndSceneGeometry();

    BeginBitmap();

    // The logo shows over the picture as it did over the camera tour.
    const bool tourMode = true;
    {
        g_fMULogoAlpha += 0.02f;
        if (g_fMULogoAlpha > 10.0f) g_fMULogoAlpha = 10.0f;
    }

    // The text state the native lines set, which RenderInfomation() below draws with.
    g_pRenderText->SetFont(g_hFont);

    InputTextWidth = 256;
    g_pRenderText->SetTextColor(255, 255, 255, 255);
    g_pRenderText->SetBgColor(0, 0, 0, 128);

    wchar_t version[100];
    swprintf_s(version, 100, I18N::Game::VerS, m_ExeVersion);

    // The logo and the bottom lines in RmlUi (login_scene.rml); natively without RmlUi.
    if (!Scenes::LoginOverlay::Render(tourMode, g_fMULogoAlpha, I18N::Game::CCopyright2001Webzen,
                                      I18N::Game::AllRightsReserved, version))
    {
        RenderLoginSceneLinesNative(tourMode, version);
    }

    RenderInfomation();

#ifdef ENABLE_EDIT
    RenderDebugWindow();
#endif

    // Handle option window in login/character scenes (can't use full g_pNewUISystem update).
    // Update() (not just UpdateMouseEvent()/UpdateKeyEvent()) is required here too -- it's what
    // runs SyncRmlModel(), which pushes this window's own C++ state (tab/row labels, checkbox
    // values, everything bound via {{}}) into its RmlUi data model. Without it, every bound field
    // stays at its default-constructed empty value and the window renders with no text at all
    // (found live: fixed the moment CManager::Update() started running this window normally, i.e.
    // after reaching a scene that pumps the full g_pNewUISystem update -- the model, once
    // populated there, stays populated even back in a scene that skips this call again, which is
    // why the symptom didn't reappear after visiting one such scene). Same order the full
    // CManager sweep uses (UpdateMouseEvent -> UpdateKeyEvent -> Update -> Render).
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_OPTION))
    {
        g_pOption->UpdateMouseEvent();
        g_pOption->UpdateKeyEvent();
        g_pOption->Update();
        g_pOption->Render();
    }

    // The "Remember Password" prompt (the one dialog that used to need a manual g_MessageBox
    // pump here, since the login scene skips the full NewUI update) is now its own RmlUi
    // document -- see UI/Windows/RememberPasswordPrompt.cpp. It's already driven by
    // RmlUiRuntime's own pre-submit callback, fired unconditionally every frame regardless of
    // scene, so no equivalent manual pump is needed for it here.

    EndBitmap();

    EndOpengl();

    return true;
}
