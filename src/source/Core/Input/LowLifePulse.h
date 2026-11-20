// The pad's warning that life runs low (pad round 7, A3): two short pulses, a heartbeat, as life falls
// to a quarter or less. Once felt it waits until life is back at 40 % or more, and a pair comes at most
// once every 10 seconds, so a fight near the line does not buzz on and on. Never while dead, and never
// on the first reading (a hero who logs in or comes back hurt). A blow that shakes the pad in the
// frame life crosses the line holds the first beat until its shake ended, so the two stay apart.
#pragma once

#include <chrono>
#include <cstdint>

namespace Core::Input::Gamepad
{
class LowLifePulse
{
public:
    using Clock = std::chrono::steady_clock;

    // Life at or under this share of the most arms the pair as it falls there.
    static constexpr float ArmShare = 0.25f;
    // Life back at or over this share lets the next fall pulse again.
    static constexpr float ReArmShare = 0.4f;
    static constexpr std::chrono::milliseconds SecondDelay{180};
    static constexpr std::chrono::seconds PairGap{10};

    enum class Pulse : std::uint8_t
    {
        None,
        First,
        Second,
    };

    // Life's share of the most now (0 .. 1), once a frame while the pad plays: the pulse to feel now.
    // `busyUntil`: when a shake this frame started (a blow's) ends, gap included; a time past with none.
    // A pair that starts this frame begins then, its second beat SecondDelay after the first.
    [[nodiscard]] Pulse Note(float lifeShare, bool dead, Clock::time_point now, Clock::time_point busyUntil = {});

    // Not playing (the pad put down, out of the world): the next reading is a first one again.
    void Forget();

private:
    bool m_Known = false;
    float m_LastShare = 0.f;
    bool m_Armed = true;
    bool m_FirstDue = false;
    Clock::time_point m_FirstAt{};
    bool m_SecondDue = false;
    Clock::time_point m_SecondAt{};
    bool m_Paired = false;
    Clock::time_point m_LastPair{};
};
} // namespace Core::Input::Gamepad
