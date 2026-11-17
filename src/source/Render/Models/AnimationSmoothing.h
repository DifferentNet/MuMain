// AnimationSmoothing.h: smoother character animation in the Remaster (desktop, HD textures installed),
// from the player's [Render] SmoothAnimation and AnimationFadeMs settings.
//
//   Between keys: a model's bones normally move in straight lines from one key of its action to the
//   next, which turns sharply at every key. Characters, monsters, NPCs and mounts instead follow a
//   curve through the keys before and after (a Hermite spline whose slopes are limited so it never
//   swings past either key and holds still where two keys are the same). Only the bone pose changes:
//   the frame counters the game logic reads stay as they are.
//
//   Between actions: a new action normally snaps to its first key. A character of the main scene
//   instead fades from the pose it showed into the new action over AnimationFadeMs (and at most
//   until the new action reaches its third key), while the new action's first key is stretched over
//   two keys so the fade does not hold it still.
//
// With the setting off (and in the Classic and browser clients) the bones are built exactly as before.
#pragma once

#include <algorithm>
#include <cstdint>

namespace Render::AnimationSmoothing
{

// [Render] AnimationFadeMs bounds and default.
inline constexpr int kDefaultFadeMs = 160;
inline constexpr int kMinFadeMs = 0;
inline constexpr int kMaxFadeMs = 300;

// A spline needs a key before and after its segment: actions with fewer keys stay linear.
inline constexpr int kMinSplineKeys = 3;
// An action counts as a cycle when the step from its last shown key back to its first is at most
// this many times its largest step between two keys.
inline constexpr float kCyclicStepFactor = 1.5f;
// A spline slope is capped at this many times the step of its segment.
inline constexpr float kMaxSlopeFactor = 3.0f;
// During a fade the new action's first key segment is stretched over this many keys.
inline constexpr float kWarpKeys = 2.0f;

// What BMD::Animation() reads to pick two keys: PriorAction, the action, PriorAnimationFrame and
// AnimationFrame (key = whole part, blend = fraction).
struct Segment
{
    unsigned short priorAction = 0;
    unsigned short action = 0;
    float priorFrame = 0.0f;
    float frame = 0.0f;
};

// A character's fade from its last pose into a new action (OBJECT::Fade).
struct AnimationFade
{
    bool active = false;
    // The model and the action the fade belongs to: anything else drawn through the object ignores it.
    int type = -1;
    unsigned short toAction = 0;
    // The pose it fades from, held still.
    Segment from;
    double startMs = 0.0;
    // The share of the new action (0..1), set once a frame by the main thread.
    float weight = 0.0f;
};

// The keys around a segment for its spline, or `regular` false when the segment stays linear.
struct SplineKeys
{
    bool regular = false;
    int prev = 0;
    int next = 0;
};

// Whether segment priorKey -> key of one action (keyCount keys) is a step a spline can follow, and the
// keys before and after it. lockPositions and holdAtEnd are the action's Action_t LockPositions and
// Loop; a cycle (cyclic, not holding at its end) wraps its neighbours around, else they stop at the
// first and last keys and the step from the last key back to the first stays linear.
[[nodiscard]] SplineKeys FindSplineKeys(int priorKey, int key, int keyCount, bool lockPositions, bool holdAtEnd,
                                        bool cyclic);

// A value between keys p1 (s = 0) and p2 (s = 1), p0 and p3 the keys before and after: a Hermite curve
// whose slopes are limited so it stays within [p1, p2] and holds still when p1 == p2.
[[nodiscard]] float LimitedHermite(float p0, float p1, float p2, float p3, float s);

// The rotation between unit quaternions q1 (s = 0) and q2 (s = 1), q0 and q3 the keys before and
// after, as LimitedHermite on each component on one hemisphere, normalised.
void SplineQuaternion(const float q0[4], const float q1[4], const float q2[4], const float q3[4], float s,
                      float out[4]);

// The angle (radians) between two unit quaternions' rotations.
[[nodiscard]] float RotationStep(const float q1[4], const float q2[4]);

// Whether an action whose largest bone step from last key to first is wrapStep, and largest step
// between two neighbouring keys largestStep, plays as a cycle.
[[nodiscard]] inline bool IsCyclic(float wrapStep, float largestStep)
{
    return wrapStep <= kCyclicStepFactor * largestStep;
}

// How far a fade has come (0..1): by time, or by the new action's frame, whichever is further.
[[nodiscard]] float FadeProgress(double elapsedMs, int fadeMs, float frame);

// The new action's share of the pose at that progress (eases in and out).
[[nodiscard]] inline float FadeWeight(float progress)
{
    const float x = std::clamp(progress, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// Whether the new action's start is stretched (see the top of the file).
[[nodiscard]] inline bool WarpsStart(float frame, int keyCount)
{
    return keyCount >= kMinSplineKeys && frame < kWarpKeys;
}

// Whether a fade is over: once the stretched start has reached the action's own frames, or, for an
// action too short to stretch, once its progress is complete.
[[nodiscard]] inline bool FadeFinished(float progress, float frame, int keyCount)
{
    return keyCount >= kMinSplineKeys ? frame >= kWarpKeys : progress >= 1.0f;
}

// The segment a fading character shows of its new action: the stretched first key segment while it
// warps, else the frames as they are.
[[nodiscard]] Segment FadeTarget(const Segment& legacy, int keyCount);

// The setting for the saved values: on only where available (the Remaster PC client).
[[nodiscard]] inline bool Resolve(int smoothAnimation, bool available)
{
    return available && smoothAnimation == 1;
}

[[nodiscard]] inline int ClampFadeMs(int fadeMs)
{
    return std::clamp(fadeMs, kMinFadeMs, kMaxFadeMs);
}

// Engine side (AnimationSmoothingSetting.cpp).

// Whether this client can use it: the Remaster PC client (HD textures installed), never the browser.
[[nodiscard]] bool Available();
// The [Render] settings, at start-up and after a change.
void ApplyFromConfig();
// Whether the bones follow the splines and fades right now.
[[nodiscard]] bool Enabled();
// The fade length in use, ms (0 = actions switch as before).
[[nodiscard]] int FadeMs();
// Saves [Render] SmoothAnimation (Options > Graphics, $anim smooth) and applies it at once.
void Choose(bool enabled);
// Saves [Render] AnimationFadeMs ($anim fade) and applies it at once.
void ChooseFadeMs(int fadeMs);
// Tests only: the state without the config file.
void SetForTests(bool enabled, int fadeMs);

} // namespace Render::AnimationSmoothing
