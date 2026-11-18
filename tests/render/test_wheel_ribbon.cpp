#include <doctest.h>

#include "Render/Effects/WheelRibbon.h"

#include <cmath>
#include <cstdint>

namespace
{
namespace Wheel = Render::Effects::WheelRibbon;
using doctest::Approx;

constexpr float kPi = 3.14159265358979f;

// AngleMatrix of (0, 0, yawDegrees): a turn about z, as the engine builds it.
void YawMatrix(const float yawDegrees, float matrix[3][4])
{
    const float radians = yawDegrees * kPi / 180.0f;
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    const float rows[3][4] = {{c, -s, 0.0f, 0.0f}, {s, c, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            matrix[row][column] = rows[row][column];
        }
    }
}
} // namespace

TEST_CASE("the band's head is where the move handler drew the lead weapon [render][wheel_ribbon]")
{
    // At 25 FPS the angle stepped a whole tick after the weapon was placed.
    CHECK(Wheel::HeadDegrees(-36.0f, 1.0f) == Approx(-18.0f));
    // At 144 FPS a sixth of it.
    CHECK(Wheel::HeadDegrees(-36.0f, 25.0f / 144.0f) == Approx(-36.0f + 18.0f * 25.0f / 144.0f));
    CHECK(Wheel::HeadDegrees(10.0f, 0.0f) == Approx(10.0f));
}

TEST_CASE("the arc grows with the swept angle and stops at 90 degrees [render][wheel_ribbon]")
{
    // Not moved yet, or moved once: nothing swept behind the drawn weapon.
    CHECK(Wheel::Arc(25.0f, 1.0f) == Approx(0.0f));
    CHECK(Wheel::Arc(24.0f, 1.0f) == Approx(0.0f));
    // Two moves at 25 FPS: one tick of arc between the drawn weapon and its start.
    CHECK(Wheel::Arc(23.0f, 1.0f) == Approx(18.0f));
    CHECK(Wheel::Arc(20.0f, 1.0f) == Approx(72.0f));
    CHECK(Wheel::Arc(19.0f, 1.0f) == Approx(Wheel::kArcDegrees));
    CHECK(Wheel::Arc(5.0f, 1.0f) == Approx(Wheel::kArcDegrees));
    // At 144 FPS the same swept arc at the same age.
    const float frameFactor = 25.0f / 144.0f;
    CHECK(Wheel::Arc(23.0f, frameFactor) == Approx(18.0f * (2.0f - frameFactor)));
    // Never negative on the first frame.
    CHECK(Wheel::Arc(25.0f, frameFactor) == Approx(0.0f));
}

TEST_CASE("the band's pieces follow the effect level [render][wheel_ribbon]")
{
    // Level 4 (8 pieces a span): 5 degrees a piece, 18 at the full arc.
    CHECK(Wheel::Pieces(Wheel::kArcDegrees, 8) == 18);
    // Level 3 (4 pieces a span): 10 degrees a piece, 9 at the full arc.
    CHECK(Wheel::Pieces(Wheel::kArcDegrees, 4) == 9);
    // A short arc takes at least one piece.
    CHECK(Wheel::Pieces(1.0f, 8) == 1);
    CHECK(Wheel::Pieces(18.0f, 8) == 4);
    // Off (levels 0-2) or nothing swept: no band.
    CHECK(Wheel::Pieces(Wheel::kArcDegrees, 0) == 0);
    CHECK(Wheel::Pieces(0.0f, 8) == 0);
    // More pieces than level 4 never overrun the band.
    CHECK(Wheel::Pieces(Wheel::kArcDegrees, 64) == Wheel::kMaxPieces);
}

TEST_CASE("the band fades over the ghost's last five ticks and toward its end [render][wheel_ribbon]")
{
    CHECK(Wheel::EndFade(25.0f) == Approx(1.0f));
    CHECK(Wheel::EndFade(5.0f) == Approx(1.0f));
    CHECK(Wheel::EndFade(2.5f) == Approx(0.5f));
    CHECK(Wheel::EndFade(0.0f) == Approx(0.0f));
    CHECK(Wheel::EndFade(-1.0f) == Approx(0.0f));

    CHECK(Wheel::Light(0.0f, 1.0f) == Approx(1.0f));
    CHECK(Wheel::Light(0.5f, 1.0f) == Approx(0.5f));
    CHECK(Wheel::Light(1.0f, 1.0f) == Approx(0.0f));
    CHECK(Wheel::Light(0.0f, 0.5f) == Approx(0.5f));
}

TEST_CASE("the band lies on the ghost weapons' orbit, flat, at the weapon's height [render][wheel_ribbon]")
{
    CHECK(Wheel::Radius(false) == Approx(150.0f));
    CHECK(Wheel::Radius(true) == Approx(180.0f));

    const float centre[3] = {1000.0f, 2000.0f, 50.0f};
    float matrix[3][4];
    float point[3];

    // Angle 0: (0, -r, 0), as the move handler places the ghost.
    YawMatrix(0.0f, matrix);
    Wheel::Point(matrix, centre, 150.0f, point);
    CHECK(point[0] == Approx(1000.0f));
    CHECK(point[1] == Approx(1850.0f));
    CHECK(point[2] == Approx(50.0f + Wheel::kHeight));

    // Angle 90: (r, 0, 0).
    YawMatrix(90.0f, matrix);
    Wheel::Point(matrix, centre, 150.0f, point);
    CHECK(point[0] == Approx(1150.0f));
    CHECK(point[1] == Approx(2000.0f).epsilon(0.0001));
    CHECK(point[2] == Approx(50.0f + Wheel::kHeight));

    // Every point of the band keeps its radius and height.
    for (float degrees = -180.0f; degrees <= 180.0f; degrees += 15.0f)
    {
        YawMatrix(degrees, matrix);
        Wheel::Point(matrix, centre, 195.0f, point);
        const float dx = point[0] - centre[0];
        const float dy = point[1] - centre[1];
        CHECK(std::sqrt(dx * dx + dy * dy) == Approx(195.0f));
        CHECK(point[2] == Approx(50.0f + Wheel::kHeight));
    }
}

TEST_CASE("the band's head plus its arc is the angle the lead ghost started at [render][wheel_ribbon]")
{
    constexpr float start = 40.0f;
    const float frameFactors[] = {1.0f, 0.5f, 25.0f / 60.0f, 25.0f / 144.0f, 25.0f / 240.0f};
    for (const float frameFactor : frameFactors)
    {
        // Each frame the move handler steps the angle and the life by this frame's share of a tick.
        float angleZ = start;
        float lifeTime = Wheel::kLifeTicks;
        while (true)
        {
            angleZ -= Wheel::kDegreesPerTick * frameFactor;
            lifeTime -= frameFactor;
            const float swept = start - Wheel::HeadDegrees(angleZ, frameFactor);
            if (swept > Wheel::kArcDegrees)
            {
                break;
            }
            CAPTURE(frameFactor);
            CAPTURE(lifeTime);
            CHECK(Wheel::HeadDegrees(angleZ, frameFactor) + Wheel::Arc(lifeTime, frameFactor) ==
                  Approx(start).epsilon(0.0001));
        }
    }
}

TEST_CASE("only the first ghost a cast lays is its lead, whatever the frame rate [render][wheel_ribbon]")
{
    // A cast starts with CreateEffect's default serial: no ghost laid.
    const Wheel::LaidGhost first = Wheel::LayGhost(Wheel::kNoGhostLaid);
    CHECK(first.castMark == Wheel::kGhostsLaid);
    CHECK(Wheel::IsLeadGhost(first.ghostMark));

    const Wheel::LaidGhost second = Wheel::LayGhost(first.castMark);
    CHECK(second.castMark == Wheel::kGhostsLaid);
    CHECK_FALSE(Wheel::IsLeadGhost(second.ghostMark));

    // Above 25 FPS a cast lays 0 to several ghosts a tick at random; exactly one is the lead.
    const int laidPerFrame[] = {0, 2, 1, 0, 3, 1, 1, 0, 2};
    std::uint8_t castMark = Wheel::kNoGhostLaid;
    int leads = 0;
    int laidSoFar = 0;
    for (const int laid : laidPerFrame)
    {
        for (int ghost = 0; ghost < laid; ++ghost)
        {
            const Wheel::LaidGhost result = Wheel::LayGhost(castMark);
            castMark = result.castMark;
            if (Wheel::IsLeadGhost(result.ghostMark))
            {
                ++leads;
                CHECK(laidSoFar == 0);
            }
            ++laidSoFar;
        }
    }
    CHECK(leads == 1);

    // CreateEffect's default serial on any other effect is no lead.
    CHECK_FALSE(Wheel::IsLeadGhost(0));
}
