#include "stdafx.h"

#include "Engine/Object/CharacterAnimationFade.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/SceneCore.h"

#include <cstdint>

namespace Engine::Object::Fade
{
namespace
{
namespace Smooth = Render::AnimationSmoothing;

// A fade interrupted with less of the new action than this keeps fading from its old pose.
constexpr float kKeepOldPoseBelow = 0.5f;

// o is the object of one of the client's characters (not a preview, clone or mount).
bool IsClientCharacter(const OBJECT& o)
{
    if (CharactersClient == nullptr)
    {
        return false;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(&o);
    const auto begin = reinterpret_cast<std::uintptr_t>(CharactersClient);
    const auto end = reinterpret_cast<std::uintptr_t>(CharactersClient + MAX_CHARACTERS_CLIENT);
    if (address < begin || address >= end)
    {
        return false;
    }
    const std::size_t index = (address - begin) / sizeof(CHARACTER);
    return &CharactersClient[index].Object == &o;
}

bool Fades(const OBJECT& o)
{
    return Smooth::Enabled() && Smooth::FadeMs() > 0 && SceneFlag == MAIN_SCENE && o.Visible &&
           IsClientCharacter(o) && Models[o.Type].SmoothKeys;
}

int KeyCount(const BMD& model, unsigned short action)
{
    return action < model.NumActions ? model.Actions[action].NumAnimationKeys : 0;
}
} // namespace

void Begin(OBJECT& o, unsigned short action, bool blending)
{
    if (!blending || !Fades(o))
    {
        o.Fade.active = false;
        return;
    }
    Start(o, action, WorldTime);
}

void Start(OBJECT& o, unsigned short action, double nowMs)
{
    Smooth::AnimationFade& fade = o.Fade;
    const Smooth::Segment shown{o.PriorAction, o.CurrentAction, o.PriorAnimationFrame, o.AnimationFrame};
    const bool interrupted = fade.active && fade.type == o.Type && fade.toAction == o.CurrentAction;
    if (!interrupted)
    {
        fade.from = shown;
    }
    else if (fade.weight >= kKeepOldPoseBelow)
    {
        fade.from = Smooth::FadeTarget(shown, KeyCount(Models[o.Type], o.CurrentAction));
    }
    fade.active = true;
    fade.type = o.Type;
    fade.toAction = action;
    fade.startMs = nowMs;
    fade.weight = 0.f;
}

void Tick(OBJECT& o, bool played)
{
    Smooth::AnimationFade& fade = o.Fade;
    if (!fade.active)
    {
        return;
    }
    const BMD& model = Models[o.Type];
    // An action set outside SetAction(), another model, the end of a held action or the setting turned off.
    if (!played || fade.type != o.Type || fade.toAction != o.CurrentAction || o.CurrentAction >= model.NumActions ||
        !Smooth::Enabled() || Smooth::FadeMs() <= 0)
    {
        fade.active = false;
        return;
    }
    const int keys = KeyCount(model, o.CurrentAction);
    const float progress = Smooth::FadeProgress(WorldTime - fade.startMs, Smooth::FadeMs(), o.AnimationFrame);
    if (Smooth::FadeFinished(progress, o.AnimationFrame, keys))
    {
        fade.active = false;
        return;
    }
    fade.weight = Smooth::FadeWeight(progress);
}

const Smooth::AnimationFade* For(const OBJECT& o, unsigned short action)
{
    const Smooth::AnimationFade& fade = o.Fade;
    if (!fade.active || fade.type != o.Type || fade.toAction != action || !Smooth::Enabled())
    {
        return nullptr;
    }
    return &fade;
}
} // namespace Engine::Object::Fade
