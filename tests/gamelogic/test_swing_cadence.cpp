#include "stdafx.h"
#include "doctest.h"

#include "Engine/Object/SwingCadence.h"

#include <algorithm>
#include <array>

namespace
{
    constexpr double ReferenceFps = 25.0;
    constexpr float SwingKeys = 7.f;            // a sword swing's length in animation keys
    constexpr float SwingBaseSpeed = 0.25f;     // PLAYER_ATTACK_SWORD_RIGHT1's base speed (SetAttackSpeed)
    constexpr float SpeedPerAttackSpeed = 0.004f;
    constexpr double SimulatedSeconds = 20.0;

    // Plays back-to-back swings the way the game does: a swing that ends on one frame is followed by the
    // next one starting on the next frame. Returns swings a second.
    double SimulateSwingsPerSecond(double fps, int attackSpeed, bool carry)
    {
        Engine::Object::SwingCadence cadence;
        const float playSpeed = SwingBaseSpeed + SpeedPerAttackSpeed * static_cast<float>(attackSpeed);
        const float frameFactor = static_cast<float>(ReferenceFps / fps);
        const int frames = static_cast<int>(fps * SimulatedSeconds);

        int swings = 0;
        bool swinging = false;
        float key = 0.f;
        for (int frame = 0; frame < frames; ++frame)
        {
            if (!swinging)
            {
                key = carry ? cadence.TakeStartKey(playSpeed, static_cast<unsigned int>(frame)) : 0.f;
                swinging = true;
                ++swings;
            }

            key += playSpeed * frameFactor;
            if (key >= SwingKeys)
            {
                cadence.RecordSwingEnd(key - SwingKeys, playSpeed, frameFactor, static_cast<unsigned int>(frame));
                swinging = false;
            }
        }

        return swings / SimulatedSeconds;
    }

    double AnimationSwingsPerSecond(int attackSpeed)
    {
        return ReferenceFps * (SwingBaseSpeed + SpeedPerAttackSpeed * attackSpeed) / SwingKeys;
    }
}

TEST_CASE("Back-to-back swings follow the animation, not the frame rate [gamelogic][timing]")
{
    for (const double fps : std::array{30.0, 60.0, 120.0, 144.0, 175.0, 240.0})
    {
        for (const int attackSpeed : std::array{200, 1000, 1500, 2000, 3300})
        {
            CAPTURE(fps);
            CAPTURE(attackSpeed);
            const double expected = std::min(AnimationSwingsPerSecond(attackSpeed), fps);
            CHECK(SimulateSwingsPerSecond(fps, attackSpeed, true) == doctest::Approx(expected).epsilon(0.03));
        }
    }
}

TEST_CASE("Without the carry, 60 fps loses swings at high attack speed [gamelogic][timing]")
{
    // The bug this fixes: 20 swings a second at 60 fps and attack speed 2000, against about 29.
    CHECK(SimulateSwingsPerSecond(60.0, 2000, false) == doctest::Approx(20.0).epsilon(0.02));
    CHECK(SimulateSwingsPerSecond(60.0, 2000, true) == doctest::Approx(AnimationSwingsPerSecond(2000)).epsilon(0.03));
}

TEST_CASE("A swing never runs faster than one a frame [gamelogic][timing]")
{
    CHECK(SimulateSwingsPerSecond(60.0, 6000, true) <= 60.0);
}

TEST_CASE("The carry goes only to a swing that starts right away, once [gamelogic][timing]")
{
    Engine::Object::SwingCadence cadence;
    cadence.RecordSwingEnd(1.f, 4.f, 0.5f, 100);
    CHECK(cadence.TakeStartKey(8.f, 101) == doctest::Approx(2.f));
    CHECK(cadence.TakeStartKey(8.f, 101) == 0.f);

    cadence.RecordSwingEnd(1.f, 4.f, 0.5f, 200);
    CHECK(cadence.TakeStartKey(8.f, 203) == 0.f);

    cadence.RecordSwingEnd(10.f, 4.f, 0.5f, 300);  // more than one frame of overshoot: at most one frame
    CHECK(cadence.TakeStartKey(4.f, 300) == doctest::Approx(2.f));
}
