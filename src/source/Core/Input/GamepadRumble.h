// Which shake of the pad plays: SDL's rumble replaces the one under way, so a short light tick (a
// target picked) would cut a hard blow's shake short. A weaker shake that would end while a stronger
// one still runs is dropped; an equal or stronger one takes over, and so does a weaker one that runs
// on past it (a level up's swell after a blow).
#pragma once

#include <chrono>

namespace Core::Input::Gamepad
{
class RumbleArbiter
{
public:
    using Clock = std::chrono::steady_clock;

    // Whether a shake of these motor strengths (each 0 .. 1) and length may play now.
    [[nodiscard]] bool Allows(float low, float high, int milliseconds, Clock::time_point now) const;

    // The shake that plays: it counts as the one under way (once the pad took it).
    void Record(float low, float high, int milliseconds, Clock::time_point now);

    // Allows, and if so Record.
    bool Take(float low, float high, int milliseconds, Clock::time_point now);

private:
    float m_Strength = 0.f;
    Clock::time_point m_Until{};
};
} // namespace Core::Input::Gamepad
