#include "stdafx.h"

#include "Engine/Object/SwingCadence.h"

#include <algorithm>

namespace Engine::Object
{
    void SwingCadence::RecordSwingEnd(float overshootKeys, float playSpeed, float frameFactor, unsigned int frame)
    {
        m_hasCarry = false;
        if (playSpeed <= 0.f || frameFactor <= 0.f || overshootKeys <= 0.f)
        {
            return;
        }

        // The overshoot happened within this frame, so it is never more than one frame of time. A larger
        // value means the animation wrapped round in an unusual way; keep at most one frame of it.
        m_carryReferenceFrames = std::min(overshootKeys / playSpeed, frameFactor);
        m_endFrame = frame;
        m_hasCarry = true;
    }

    float SwingCadence::TakeStartKey(float playSpeed, unsigned int frame)
    {
        if (!m_hasCarry)
        {
            return 0.f;
        }

        m_hasCarry = false;
        if (playSpeed <= 0.f || frame - m_endFrame > MaxFramesBetweenSwings)
        {
            return 0.f;
        }

        return m_carryReferenceFrames * playSpeed;
    }

    SwingCadence& HeroSwingCadence()
    {
        static SwingCadence cadence;
        return cadence;
    }
}
