// Which shake of the pad plays: SDL's rumble replaces the one under way, so a short light tick (a
// target picked) would cut a hard blow's shake short. A weaker shake while a stronger one still runs
// is dropped; an equal or stronger one takes over.
#pragma once

#include <chrono>

namespace Core::Input::Gamepad
{
class RumbleArbiter
{
public:
    using Clock = std::chrono::steady_clock;

    // Whether a shake of these motor strengths (each 0 .. 1) and length plays now; it then counts as
    // the one under way.
    bool Take(float low, float high, int milliseconds, Clock::time_point now);

private:
    float m_Strength = 0.f;
    Clock::time_point m_Until{};
};
} // namespace Core::Input::Gamepad
