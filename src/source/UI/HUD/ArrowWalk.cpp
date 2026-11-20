#include "stdafx.h"
#include "UI/HUD/ArrowWalk.h"

#include "Core/Input/ArrowSteer.h"
#include "Core/Input/KeyState.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Touch/TouchActions.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Scenes/SceneCore.h"
#include "UI/Dialogs/ModalDialogs.h"
#include "UI/HUD/GamepadControls.h"
#include "UI/Mobile/InterfaceMode.h"

#ifdef _EDITOR
#include "Camera/CameraManager.h"
#include "Camera/CameraMode.h"
#endif

#include <optional>

namespace
{
namespace Arrows = Core::Input::ArrowSteer;

// The arrows' way to the screen's walk, as the pad's stick (GamepadControls' StickToScreen).
constexpr float ArrowsToScreen = 100.f;

// Debug builds: the arrows move the hero by the editor's camera (DefaultCamera::HandleEditorMode).
#ifdef ENABLE_EDIT2
constexpr bool ArrowsMoveEditorHero = true;
#else
constexpr bool ArrowsMoveEditorHero = false;
#endif

struct State
{
    // What Update chose for LateUpdate: the way to walk, or the end of the walk.
    std::optional<Arrows::Direction> walk;
    bool stop = false;
    // The arrows walk the hero, or wait to be let go after a gate closed on their walk.
    bool steering = false;
    bool waiting = false;
};
State g_state;

bool IsInWorld()
{
    return SceneFlag == MAIN_SCENE && LoadingWorld < 30 && Hero != nullptr;
}

// Whether the arrows may walk now, held as they are: nothing else wants the keys or the hero.
bool ArrowsMayWalk()
{
    if (!IsInWorld() || Hero->Dead != 0 || UI::Mobile::IsGamepadInUse())
        return false;
    if (Core::Input::IsKeyDown(VK_CONTROL) || Core::Input::IsKeyDown(VK_MENU) || Core::Input::IsKeyDown(VK_SHIFT))
        return false;
    const RmlUiRuntime& rml = RmlUiRuntime::Instance();
    if (rml.IsTextInputActive() || rml.GetKeyOwningDocument() != nullptr)
        return false;
#ifdef _EDITOR
    if (CameraManager::Instance().GetCurrentMode() == CameraMode::FreeFly)
        return false;
#endif
    return !UI::HUD::GamepadControls::IsGameWindowOpen() && !UI::Dialogs::Modal::IsOpen();
}

Arrows::Direction ArrowsHeld()
{
    return Arrows::FromKeys(Core::Input::IsKeyDown(VK_LEFT), Core::Input::IsKeyDown(VK_RIGHT),
                            Core::Input::IsKeyDown(VK_UP), Core::Input::IsKeyDown(VK_DOWN));
}
} // namespace

void UI::HUD::ArrowWalk::Update()
{
    g_state.walk.reset();
    g_state.stop = false;
    if constexpr (ArrowsMoveEditorHero)
        return;
    const Arrows::Direction direction = ArrowsHeld();
    if (!Arrows::Any(direction))
    {
        // All let go: the walk ends, and one that waited past a gate may go again.
        g_state.stop = g_state.steering || g_state.waiting;
        return;
    }
    if (!ArrowsMayWalk())
    {
        if (g_state.steering)
        {
            GameLogic::Touch::PauseSteering();
            g_state.steering = false;
            g_state.waiting = true;
        }
        return;
    }
    // Another device walks the hero (the touch stick, the pad's stick): the arrows leave it be.
    if (!g_state.steering && !g_state.waiting && GameLogic::Touch::IsSteering())
        return;
    g_state.walk = direction;
}

void UI::HUD::ArrowWalk::LateUpdate()
{
    if (g_state.stop)
    {
        GameLogic::Touch::StopSteering();
        g_state.steering = false;
        g_state.waiting = false;
        return;
    }
    if (!g_state.walk)
        return;
    // Past a gate the push waits to be let go: Steer itself keeps the hero standing then.
    GameLogic::Touch::Steer(g_state.walk->x * ArrowsToScreen, g_state.walk->y * ArrowsToScreen);
    g_state.steering = true;
    g_state.walk.reset();
}
