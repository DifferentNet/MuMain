#include "stdafx.h"
#include "Scenes/LowFpsHint.h"

#include "Core/Performance/LowFpsDetector.h"
#include "Core/Text/Utf8.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigValidation.h"
#include "Render/Look/GraphicsPresetSetting.h"
#include "Render/Look/LowFpsHintPolicy.h"
#include "UI/HUD/GraphicsHint.h"

#ifndef __EMSCRIPTEN__
#include "Core/Utilities/Benchmark/BenchRecorder.h"
#include "I18N/All.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/SceneCore.h"
#include "UI/Chat/MessageType.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Dialogs/MessageBox.h"
#include "World/MapInfra/MapManager.h"
#endif

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#ifndef __EMSCRIPTEN__
extern double WorldTime;
extern int LoadingWorld;
extern bool g_bHasInputFocus;
extern bool g_bWindowMinimized;
#endif

namespace
{
#ifndef __EMSCRIPTEN__
using Core::Performance::LowFpsDetector;
using Core::Performance::LowFpsFrameResult;
using Core::Performance::LowFpsRules;
namespace Policy = Render::Look::LowFpsHint;

// LoadingWorld counts down from here after a map entry; under it the world is ready (Scenes/MainScene.cpp).
constexpr int WorldReadyBelow = 30;
// An unanswered toast goes away after this long in front of the player, and that counts as Keep.
constexpr double AutoHideMs = 30000.0;
// `$fpshint fast`: short windows, warm-up and quiet, to see the hint without waiting a minute.
constexpr double FastWindowMs = 3000.0;
constexpr int FastMinFrames = 10;

struct HintState
{
    LowFpsDetector detector;
    bool fast = false;
    // `$fpshint sim <fps>`: every frame counts as this frame rate (0 = off).
    float simulatedFps = 0.0f;
    float lastFrameMs = 0.0f;

    double lastWorldTimeMs = -1.0;
    bool wasInMainScene = false;
    bool wasSteady = false;
    int lastWorld = -1;
    std::optional<GraphicsPreset> lastPreset;

    // The hint is due and waits until nothing else is in the way.
    bool pending = false;
    bool forced = false;
    int pendingFps = 0;

    // The toast is up, offering shownTarget, for shownForMs of the player's time.
    bool toastUp = false;
    GraphicsPreset shownTarget = GraphicsPreset::Low;
    double shownForMs = 0.0;

    // The median of the last finished window (the frame rate the toast quotes).
    float lastMedianFps = 0.0f;

    // After the player took a lower preset: the window before it, to see whether the step helped.
    bool checkingStep = false;
    float fpsBeforeStep = 0.0f;
    // The step didn't help (the game is held back by something the preset doesn't change): no more hints.
    bool stoppedForSession = false;
};

HintState& State()
{
    static HintState state;
    return state;
}

std::shared_ptr<spdlog::logger> Log()
{
    return mu::log::Get("render");
}

LowFpsRules CurrentRules(const HintState& state)
{
    LowFpsRules rules;
    if (state.fast)
    {
        rules.windowMs = FastWindowMs;
        rules.warmupMs = FastWindowMs;
        rules.quietMs = FastWindowMs;
        rules.minFramesPerWindow = FastMinFrames;
    }
    return rules;
}

Policy::PolicyState CurrentPolicy(const HintState& state)
{
    const GameConfig& config = GameConfig::GetInstance();
    Policy::PolicyState policy;
    policy.current = config.GetGraphicsPreset();
    policy.enabled = config.GetLowFpsHint();
    policy.kept = config.GetLowFpsHintKept();
    policy.stoppedForSession = state.stoppedForSession;
    return policy;
}

std::string PresetName(GraphicsPreset preset)
{
    return Core::Text::ToUtf8(GameConfigValidation::FormatGraphicsPreset(preset));
}

// The FPS cap the frame loop holds to: the FPS cap, else the display's rate under VSync, else none (0).
double KnownCapFps()
{
    const double target = GetTargetFps();
    if (target > 0.0)
        return target;
    if (IsVSyncEnabled())
        return GetHighestDisplayRefreshHz();
    return 0.0;
}

bool InWorld()
{
    return SceneFlag == MAIN_SCENE && LoadingWorld < WorldReadyBelow;
}

bool OptionsOpen()
{
    return g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_OPTION);
}

// Play the frame rate says something about: in the world, the window in front, no benchmark, Options closed.
bool IsSteady()
{
    return InWorld() && g_bHasInputFocus && !g_bWindowMinimized &&
           !Core::Benchmark::Recorder::Instance().IsRunning() && !OptionsOpen();
}

// Something the toast must not pop over: a message box, a confirm dialog, the chat's typing, Options, or the
// player not looking (the window in the background or minimised).
bool SomethingInTheWay()
{
    if (!InWorld() || OptionsOpen() || !g_bHasInputFocus || g_bWindowMinimized)
        return true;
    if (!g_MessageBox->IsEmpty())
        return true;
    if (mu::ui::window::g_pGenericConfirmDialog != nullptr && mu::ui::window::g_pGenericConfirmDialog->IsVisible())
        return true;
    return RmlUiRuntime::Instance().IsTextInputActive();
}

// A hint that came due and waits to be shown no longer stands once play changed (left the world, a map, preset or
// focus change, a window that was not slow). `$fpshint force` stays until shown.
void DropPending(HintState& state)
{
    if (!state.forced)
        state.pending = false;
}

void StartWarmup(HintState& state)
{
    state.detector.StartWarmup(WorldTime);
    DropPending(state);
}

void StartQuiet(HintState& state)
{
    state.detector.StartQuiet(WorldTime);
    DropPending(state);
}

// The toast's offer is still the step down from the preset chosen now (the player didn't change it by hand, to
// Custom either, while it was up).
bool OfferStillStands(const HintState& state)
{
    const std::optional<GraphicsPreset> step = Policy::LowerStep(GameConfig::GetInstance().GetGraphicsPreset());
    return step.has_value() && *step == state.shownTarget;
}

void SystemLine(const std::wstring& line)
{
    if (g_pSystemLogBox != nullptr)
        g_pSystemLogBox->AddText(line.c_str(), mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

void Lower(HintState& state, GraphicsPreset target)
{
    const GraphicsPreset before = GameConfig::GetInstance().GetGraphicsPreset();
    Render::Look::ApplyGraphicsPreset(target);
    state.lastPreset = GameConfig::GetInstance().GetGraphicsPreset();

    wchar_t line[512] = {};
    mu_swprintf_s(line, I18N::Game::GraphicsHintApplied, UI::GraphicsHint::PresetLabel(target));
    SystemLine(line);

    state.checkingStep = true;
    state.fpsBeforeStep = state.lastMedianFps;
    StartQuiet(state);
    Log()->info("Low frame rate hint: lowered {} -> {} at {:.1f} FPS", PresetName(before), PresetName(target),
                state.lastMedianFps);
}

void Keep(GraphicsPreset current)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetLowFpsHintKept(current);
    config.Save();
    Log()->info("Low frame rate hint: {} kept", PresetName(current));
}

void Never()
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetLowFpsHint(false);
    config.Save();
    Log()->info("Low frame rate hint: turned off (Options > Graphics turns it back on)");
}

// The toast is up: the player's answer, or the auto-hide. False once it has gone.
bool FollowShownToast(HintState& state, double frameMs)
{
    using UI::GraphicsHint::Answer;
    const GraphicsPreset current = GameConfig::GetInstance().GetGraphicsPreset();
    // An answer to an offer that no longer stands changes nothing: never a raise, never over Custom, and no Keep for
    // a preset the toast didn't ask about.
    const bool stands = OfferStillStands(state);
    switch (UI::GraphicsHint::TakeAnswer())
    {
    case Answer::Lower:
        if (stands)
            Lower(state, state.shownTarget);
        return false;
    case Answer::Keep:
        if (stands)
            Keep(current);
        StartQuiet(state);
        return false;
    case Answer::Never:
        Never();
        return false;
    case Answer::None:
        break;
    }

    // Each way out without an answer hides the document itself, so its visibility is really reset (a scene change
    // or a reconnect may have hidden or suspended it behind the toast's back).
    if (SceneFlag != MAIN_SCENE || OptionsOpen() || !stands)
    {
        // Back to the character list or the login, Options opened (the player chooses there), or the preset changed
        // by hand under it: the offer goes, with no answer.
        UI::GraphicsHint::Hide();
        return false;
    }
    if (!UI::GraphicsHint::IsShown())
    {
        UI::GraphicsHint::Hide();
        return false;
    }
    // Only the time the player can see it counts towards the auto-hide.
    if (g_bHasInputFocus && !g_bWindowMinimized)
        state.shownForMs += frameMs;
    if (state.shownForMs < AutoHideMs)
        return true;
    UI::GraphicsHint::Hide();
    Keep(current);
    StartQuiet(state);
    return false;
}

void ShowPending(HintState& state)
{
    if (SomethingInTheWay())
        return;
    state.pending = false;

    const Policy::PolicyState policy = CurrentPolicy(state);
    const std::optional<GraphicsPreset> target =
        state.forced ? Policy::LowerStep(policy.current) : Policy::Target(policy);
    state.forced = false;
    if (!target.has_value())
        return;

    state.shownTarget = *target;
    state.shownForMs = 0.0;
    UI::GraphicsHint::Show(*target, state.pendingFps);
    state.toastUp = UI::GraphicsHint::IsShown();
    Log()->info("Low frame rate hint: offering {} -> {} at {} FPS", PresetName(policy.current), PresetName(*target),
                state.pendingFps);
}

void OnWindowCompleted(HintState& state, const LowFpsFrameResult& result)
{
    state.lastMedianFps = result.windowMedianFps;
    if (!result.windowSlow)
        DropPending(state);
    if (!state.checkingStep)
        return;
    state.checkingStep = false;
    if (Policy::StepHelped(state.fpsBeforeStep, result.windowMedianFps))
        return;
    state.stoppedForSession = true;
    Log()->info("Low frame rate hint: the lower preset gave {:.1f} -> {:.1f} FPS (under {:.0f} %), no more hints this "
                "session",
                state.fpsBeforeStep, result.windowMedianFps, Policy::MinimumGainAfterStep * 100.0f);
}

// A scene entry, a map, preset or focus change: when measuring starts (again).
void FollowChanges(HintState& state, bool steady)
{
    const bool inMainScene = SceneFlag == MAIN_SCENE;
    if (inMainScene && !state.wasInMainScene)
    {
        StartWarmup(state);
        state.lastWorld = gMapManager.WorldActive;
    }
    if (!inMainScene && state.wasInMainScene)
        DropPending(state);
    state.wasInMainScene = inMainScene;

    if (gMapManager.WorldActive != state.lastWorld)
    {
        // Another map runs at another frame rate: a step taken on the old one can't be judged here.
        state.lastWorld = gMapManager.WorldActive;
        state.checkingStep = false;
        StartQuiet(state);
    }

    const GraphicsPreset preset = GameConfig::GetInstance().GetGraphicsPreset();
    if (state.lastPreset.has_value() && *state.lastPreset != preset)
    {
        // The player changed it by hand: a fresh start, and whatever the hint lowered is no longer being judged.
        state.checkingStep = false;
        StartQuiet(state);
    }
    state.lastPreset = preset;

    if (steady && !state.wasSteady)
        StartQuiet(state);
    state.wasSteady = steady;
}
#endif
} // namespace

namespace Scenes::LowFpsHint
{

void OnFrame()
{
#ifndef __EMSCRIPTEN__
    if (!Render::Look::RemasterGraphicsOptions())
        return;

    HintState& state = State();
    const double frameMs = state.lastWorldTimeMs < 0.0 ? 0.0 : WorldTime - state.lastWorldTimeMs;
    state.lastWorldTimeMs = WorldTime;
    if (frameMs <= 0.0)
        return;
    state.lastFrameMs = static_cast<float>(frameMs);

    if (state.toastUp)
        state.toastUp = FollowShownToast(state, frameMs);

    const bool steady = IsSteady();
    FollowChanges(state, steady);

    // A simulated frame rate stands in for every frame but a stall, which stays one.
    float measuredMs = static_cast<float>(frameMs);
    if (state.simulatedFps > 0.0f && measuredMs <= state.detector.Rules().stallFrameMs)
        measuredMs = 1000.0f / state.simulatedFps;

    const LowFpsFrameResult result = state.detector.OnFrame(WorldTime, measuredMs, steady, KnownCapFps());
    if (result.windowCompleted)
        OnWindowCompleted(state, result);
    if (result.hintDue && !state.toastUp && Policy::Target(CurrentPolicy(state)).has_value())
    {
        state.pending = true;
        // Rounded down: a window under 30 FPS never reads "about 30 FPS".
        state.pendingFps = static_cast<int>(result.windowMedianFps);
    }
    if (state.pending && !state.toastUp)
        ShowPending(state);
#endif
}

bool HandleDeveloperCommand(const std::wstring& command)
{
#ifdef __EMSCRIPTEN__
    (void)command;
    return false;
#else
    constexpr std::wstring_view Prefix = L"$fpshint";
    if (!command.starts_with(Prefix))
        return false;
    std::wstring_view argument = std::wstring_view(command).substr(Prefix.size());
    if (!argument.empty() && argument.front() != L' ')
        return false; // another command that starts with the same letters
    while (!argument.empty() && argument.front() == L' ')
        argument.remove_prefix(1);

    HintState& state = State();
    if (argument == L"force")
    {
        // Offers the next lower preset at once (still waiting for message boxes and the chat), past Keep, Never and
        // the session stop.
        state.pending = true;
        state.forced = true;
        float fps = state.lastMedianFps;
        if (fps <= 0.0f && state.lastFrameMs > 0.0f)
            fps = 1000.0f / state.lastFrameMs;
        state.pendingFps = static_cast<int>(fps);
        Log()->info("Low frame rate hint: forced");
    }
    else if (argument.starts_with(L"sim"))
    {
        std::wstring_view value = argument.substr(3);
        while (!value.empty() && value.front() == L' ')
            value.remove_prefix(1);
        state.simulatedFps = value == L"off" ? 0.0f : static_cast<float>(_wtof(std::wstring(value).c_str()));
        if (state.simulatedFps < 0.0f)
            state.simulatedFps = 0.0f;
        Log()->info("Low frame rate hint: simulated frame rate {}",
                    state.simulatedFps > 0.0f ? std::to_string(static_cast<int>(state.simulatedFps)) + " FPS"
                                              : std::string("off"));
    }
    else if (argument == L"fast")
    {
        state.fast = !state.fast;
        state.detector.SetRules(CurrentRules(state));
        state.detector.StartQuiet(WorldTime);
        Log()->info("Low frame rate hint: {} windows", state.fast ? "fast (3 s)" : "normal (20 s)");
    }
    else if (argument == L"reset")
    {
        state.detector.Reset();
        state.stoppedForSession = false;
        state.checkingStep = false;
        state.pending = false;
        state.forced = false;
        GameConfig& config = GameConfig::GetInstance();
        config.SetLowFpsHint(true);
        config.SetLowFpsHintKept(std::nullopt);
        config.Save();
        Log()->info("Low frame rate hint: reset (on, nothing kept, measuring now)");
    }
    else if (argument == L"status" || argument.empty())
    {
        const Policy::PolicyState policy = CurrentPolicy(state);
        const std::optional<GraphicsPreset> target = Policy::Target(policy);
        const double waitMs = state.detector.MeasuringFromMs() - WorldTime;
        Log()->info("Low frame rate hint: remaster {}, preset {}, on {}, kept {}, stopped {}, offers {}; steady {}, "
                    "slow windows {}, frames {}, waits {:.0f} ms, bar {:.1f} FPS, last median {:.1f} FPS, sim {:.0f}, "
                    "fast {}, toast {}, pending {}, checking step {}",
                    Render::Look::RemasterGraphicsOptions(), PresetName(policy.current), policy.enabled,
                    policy.kept.has_value() ? PresetName(*policy.kept) : std::string("none"),
                    policy.stoppedForSession, target.has_value() ? PresetName(*target) : std::string("nothing"),
                    IsSteady(), state.detector.SlowWindowsInRow(), state.detector.FramesInWindow(),
                    waitMs > 0.0 ? waitMs : 0.0, LowFpsDetector::SlowBarFps(state.detector.Rules(), KnownCapFps()),
                    state.lastMedianFps, state.simulatedFps, state.fast, state.toastUp, state.pending,
                    state.checkingStep);
    }
    else
    {
        Log()->warn("$fpshint: force, sim <fps>, sim off, fast, reset or status");
    }
    Log()->flush();
    return true;
#endif
}

} // namespace Scenes::LowFpsHint
