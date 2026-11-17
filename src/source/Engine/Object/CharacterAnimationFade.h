#pragma once

#include "Render/Models/AnimationSmoothing.h"

class OBJECT;

// A character's fade from the pose it shows into a new action (Render/Models/AnimationSmoothing.h).
// Only the bone pose fades: AnimationFrame, PriorAction and PriorAnimationFrame stay as the game sets them.
namespace Engine::Object::Fade
{
// Before SetAction() switches o to another action: starts o's fade from the pose it shows, for a visible
// character of the main scene on a smoothed model while the smoothed animation fades; otherwise (or without
// blending) o switches as before.
void Begin(OBJECT& o, unsigned short action, bool blending);
// Starts o's fade into action at nowMs without Begin()'s checks. A fade interrupted in its first half keeps
// fading from where it started, one further on fades from the pose of the action it was fading into.
void Start(OBJECT& o, unsigned short action, double nowMs);
// Once a frame on the main thread, right after o's PlayAnimation() (played: what it returned): the share of
// the new action for this frame's bones, or the fade's end.
void Tick(OBJECT& o, bool played);
// o's fade into action while it applies to o's model and that action, else null.
[[nodiscard]] const Render::AnimationSmoothing::AnimationFade* For(const OBJECT& o, unsigned short action);
} // namespace Engine::Object::Fade
