#include "Render/Effects/EchoTrail.h"

#include <algorithm>
#include <cmath>

namespace Render::Effects::EchoTrail
{
namespace
{
int SlotsOf(const Settings& settings)
{
    return std::clamp(settings.maxAlive, 1, MaxCopies);
}
} // namespace

int Trail::Advance(float x, float y, double nowMs, const Settings& settings)
{
    if (!m_hasLast)
    {
        m_lastX = x;
        m_lastY = y;
        m_hasLast = true;
        m_path = 0.f;
        return -1;
    }

    const float dx = x - m_lastX;
    const float dy = y - m_lastY;
    const float step = std::sqrt(dx * dx + dy * dy);
    if (!(step <= settings.jump))
    {
        // A teleport or a warp: the copies would hang in the air where it was. Start over from here.
        Clear();
        m_lastX = x;
        m_lastY = y;
        m_hasLast = true;
        return -1;
    }

    m_lastX = x;
    m_lastY = y;
    m_path += step;
    if (settings.spacing <= 0.f || m_path < settings.spacing)
        return -1;
    m_path = std::fmod(m_path, settings.spacing);

    // A dead slot, else the oldest copy makes room.
    const int slots = SlotsOf(settings);
    int chosen = -1;
    for (int slot = 0; slot < slots; ++slot)
    {
        if (!IsAlive(slot, nowMs, settings))
        {
            chosen = slot;
            break;
        }
    }
    if (chosen < 0)
    {
        chosen = 0;
        for (int slot = 1; slot < slots; ++slot)
        {
            if (m_copies[slot].bornMs < m_copies[chosen].bornMs)
                chosen = slot;
        }
    }

    m_copies[chosen].used = true;
    m_copies[chosen].bornMs = nowMs;
    return chosen;
}

bool Trail::IsAlive(int slot, double nowMs, const Settings& settings) const
{
    if (slot < 0 || slot >= SlotsOf(settings) || !m_copies[slot].used)
        return false;
    const double age = nowMs - m_copies[slot].bornMs;
    return age >= 0.0 && age < settings.lifetimeMs;
}

float Trail::AlphaOf(int slot, double nowMs, const Settings& settings) const
{
    if (!IsAlive(slot, nowMs, settings))
        return 0.f;
    const double age = nowMs - m_copies[slot].bornMs;
    return settings.startAlpha * static_cast<float>(1.0 - age / settings.lifetimeMs);
}

int Trail::AliveCount(double nowMs, const Settings& settings) const
{
    int count = 0;
    for (int slot = 0; slot < MaxCopies; ++slot)
    {
        if (IsAlive(slot, nowMs, settings))
            ++count;
    }
    return count;
}

float ClearanceFactor(float distance, float clearance)
{
    if (!(clearance > 0.f))
        return 1.f;
    if (!(distance > 0.f))
        return 0.f;
    return std::min(distance / clearance, 1.f);
}

void Trail::Clear()
{
    for (Copy& copy : m_copies)
        copy = {};
    m_hasLast = false;
    m_path = 0.f;
}
} // namespace Render::Effects::EchoTrail
