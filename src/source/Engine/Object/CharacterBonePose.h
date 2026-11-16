#pragma once

#include <cstdint>

class OBJECT;

// Everything a model-space BMD::Animation() call reads for one object (Parent and Translate off):
// the character pre-pass keeps it with the bones it built, so render can tell whether its own call
// would build the very same bones.
struct CharacterBonePose
{
    bool valid = false;
    int type = -1;
    unsigned short currentAction = 0;
    unsigned short priorAction = 0;
    float animationFrame = 0.f;
    float priorAnimationFrame = 0.f;
    float angle[3] = {};
    float headAngle[3] = {};
    float bodyHeight = 0.f;
    std::uint32_t modelGeneration = 0;
};

namespace Engine::Object::BonePose
{
// [UI] ReuseCharacterBones: render keeps the pre-pass bones instead of animating the character again.
extern bool g_ReuseEnabled;

// After o->BoneTransform was built from o's current inputs, model space, at the model's bodyHeight.
void Remember(OBJECT& o, float bodyHeight);
// After anything else wrote o->BoneTransform, or the buffer was replaced.
void Forget(OBJECT& o);
// o->BoneTransform holds what a model-space Animation() of o's current inputs at bodyHeight would build.
bool Matches(const OBJECT& o, float bodyHeight);
} // namespace Engine::Object::BonePose
