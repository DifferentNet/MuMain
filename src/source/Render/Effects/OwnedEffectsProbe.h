#pragma once

#include "Render/Effects/ZzzEffect.h"

#include <cstdint>

namespace Render::Effects
{
// Lets a run of DeleteEffect(Type, Owner) / DeleteJoint(Type, Target) calls for one object be skipped when they
// cannot delete anything: the object owned no effect and was the target of no joint when probed, and no effect,
// skill effect or joint was created since (every create takes a slot through PoolSlotBounds::OnTaken). Deleting
// only removes, so with nothing created there is still nothing of the object's to delete.
class OwnedEffectsProbe
{
public:
    explicit OwnedEffectsProbe(const OBJECT* object)
        : m_effectTakes(EffectTakes()), m_jointTakes(JointTakes()),
          m_ownedAtProbe(HasEffectOf(object) || HasJointOn(object))
    {
    }

    // False only when a delete for the object provably finds nothing.
    [[nodiscard]] bool MayOwn() const
    {
        return m_ownedAtProbe || EffectTakes() != m_effectTakes || JointTakes() != m_jointTakes;
    }

private:
    std::uint32_t m_effectTakes;
    std::uint32_t m_jointTakes;
    bool m_ownedAtProbe;
};
} // namespace Render::Effects
