#include "stdafx.h"

#include "Render/Models/AnimationSmoothing.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/AnimationTaskPool.h"
#include "Render/Look/GraphicsPresetSetting.h"

#include <atomic>
#include <optional>
#include <utility>

namespace Render::AnimationSmoothing
{

namespace
{
// Read by the animation workers (BMD::Animation, the pre-pass pose key), written by the main thread.
std::atomic<bool> s_enabled{false};
std::atomic<int> s_fadeMs{kDefaultFadeMs};
// The state last logged, for logging only its changes.
std::optional<std::pair<bool, int>> s_applied;

void Log()
{
    const std::pair<bool, int> state{s_enabled.load(), s_fadeMs.load()};
    if (s_applied == state)
    {
        return;
    }
    s_applied = state;
    mu::log::Get("render")->info("Smooth animation {} (fade {} ms)", state.first ? "on" : "off", state.second);
}

// The bones of a pre-pass still in flight are built and keyed with the old values: they finish first.
void Store(bool enabled, int fadeMs)
{
    AnimationTaskPool::Instance().Wait();
    s_enabled.store(enabled, std::memory_order_relaxed);
    s_fadeMs.store(fadeMs, std::memory_order_relaxed);
}
} // namespace

bool Available()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    return Render::Look::RemasterGraphicsOptions();
#endif
}

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    Store(Resolve(config.GetSmoothAnimation(), Available()), ClampFadeMs(config.GetAnimationFadeMs()));
    Log();
}

bool Enabled()
{
    return s_enabled.load(std::memory_order_relaxed);
}

int FadeMs()
{
    return s_fadeMs.load(std::memory_order_relaxed);
}

void Choose(const bool enabled)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetSmoothAnimation(enabled ? 1 : 0);
    config.Save();
    ApplyFromConfig();
}

void ChooseFadeMs(const int fadeMs)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetAnimationFadeMs(ClampFadeMs(fadeMs));
    config.Save();
    ApplyFromConfig();
}

void SetForTests(const bool enabled, const int fadeMs)
{
    Store(enabled, ClampFadeMs(fadeMs));
}

} // namespace Render::AnimationSmoothing
