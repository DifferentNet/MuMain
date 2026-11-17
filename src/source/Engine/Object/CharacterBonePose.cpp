#include "stdafx.h"

#include "CharacterBonePose.h"

#include "Engine/Object/w_ObjectInfo.h"
#include "Render/Models/ZzzBMD.h"

#include <bit>
#include <climits>

namespace Engine::Object::BonePose
{
bool g_ReuseEnabled = true;

namespace
{
// Bit for bit: the same bits make the same bones, and a NaN still equals itself.
bool SameBits(float left, float right)
{
    return std::bit_cast<std::uint32_t>(left) == std::bit_cast<std::uint32_t>(right);
}

bool SameBits(const float (&left)[3], const float (&right)[3])
{
    return SameBits(left[0], right[0]) && SameBits(left[1], right[1]) && SameBits(left[2], right[2]);
}

CharacterBonePose PoseOf(const OBJECT& o, float bodyHeight)
{
    CharacterBonePose pose;
    pose.valid = true;
    pose.type = o.Type;
    pose.currentAction = o.CurrentAction;
    pose.priorAction = o.PriorAction;
    pose.animationFrame = o.AnimationFrame;
    pose.priorAnimationFrame = o.PriorAnimationFrame;
    for (int axis = 0; axis < 3; ++axis)
    {
        pose.angle[axis] = o.Angle[axis];
        pose.headAngle[axis] = o.HeadAngle[axis];
    }
    pose.bodyHeight = bodyHeight;
    pose.modelGeneration = ModelDataGeneration();
    return pose;
}

bool SamePose(const CharacterBonePose& left, const CharacterBonePose& right)
{
    return left.type == right.type && left.currentAction == right.currentAction &&
           left.priorAction == right.priorAction && SameBits(left.animationFrame, right.animationFrame) &&
           SameBits(left.priorAnimationFrame, right.priorAnimationFrame) && SameBits(left.angle, right.angle) &&
           SameBits(left.headAngle, right.headAngle) && SameBits(left.bodyHeight, right.bodyHeight) &&
           left.modelGeneration == right.modelGeneration;
}
} // namespace

void Remember(OBJECT& o, float bodyHeight)
{
    // The pre-pass hands the action to Animation() as a short; above SHRT_MAX it would read the
    // model's own CurrentAction instead, which render does not.
    if (o.CurrentAction > SHRT_MAX)
    {
        Forget(o);
        return;
    }
    o.PrepassPose = PoseOf(o, bodyHeight);
}

void Forget(OBJECT& o)
{
    o.PrepassPose.valid = false;
}

bool Matches(const OBJECT& o, float bodyHeight)
{
    if (!o.PrepassPose.valid || !o.EnableBoneMatrix || o.BoneTransform == nullptr) return false;
    return SamePose(o.PrepassPose, PoseOf(o, bodyHeight));
}
} // namespace Engine::Object::BonePose
