#include <doctest.h>

#include "Render/Effects/ParticlesSetting.h"

#include <cmath>
#include <limits>

namespace
{
namespace Particles = Render::Effects::Particles;
using Particles::Kind;

constexpr double kPeriodMs = Particles::kPeriodSeconds * 1000.0;

// Where a particle at place `share` (of the box) lands in the world with the box centred at
// `centre`: the shader's wrap, on one axis.
float WorldPosition(float share, float centre, float boxLength)
{
    return centre + Particles::WrapShare(share - centre / boxLength) * boxLength;
}
} // namespace

TEST_CASE("the strength is ParticlesStrength over 70, clamped to 0..100 [render][particles]")
{
    CHECK(Particles::Resolve(1, 70, true, false) == doctest::Approx(1.0f));
    CHECK(Particles::Resolve(1, 35, true, false) == doctest::Approx(0.5f));
    CHECK(Particles::Resolve(1, 100, true, false) == doctest::Approx(100.0f / 70.0f));
    CHECK(Particles::Resolve(1, 250, true, false) == doctest::Approx(100.0f / 70.0f));
    CHECK(Particles::Resolve(1, -5, true, false) == 0.0f);
}

TEST_CASE("off without HD textures, on Low, for 0 or unknown values, at strength 0 [render][particles]")
{
    CHECK(Particles::Resolve(1, 70, false, false) == 0.0f);
    CHECK(Particles::Resolve(1, 70, true, true) == 0.0f);
    CHECK(Particles::Resolve(0, 70, true, false) == 0.0f);
    CHECK(Particles::Resolve(2, 70, true, false) == 0.0f);
    CHECK(Particles::Resolve(1, 0, true, false) == 0.0f);
}

TEST_CASE("a light mood value gives a kind, rounded; anything else is none [render][particles]")
{
    CHECK(Particles::KindFromValue(0.0) == Kind::None);
    CHECK(Particles::KindFromValue(1.0) == Kind::Dust);
    CHECK(Particles::KindFromValue(2.0) == Kind::Pollen);
    CHECK(Particles::KindFromValue(3.0) == Kind::Fireflies);
    CHECK(Particles::KindFromValue(4.0) == Kind::Snow);
    CHECK(Particles::KindFromValue(5.0) == Kind::Ash);
    CHECK(Particles::KindFromValue(6.0) == Kind::Embers);
    CHECK(Particles::KindFromValue(2.4) == Kind::Pollen);
    CHECK(Particles::KindFromValue(7.0) == Kind::None);
    CHECK(Particles::KindFromValue(-1.0) == Kind::None);
    CHECK(Particles::KindFromValue(std::numeric_limits<double>::quiet_NaN()) == Kind::None);
    CHECK(Particles::KindFromValue(std::numeric_limits<double>::infinity()) == Kind::None);
}

TEST_CASE("a layer's count follows the map's density and the strength, capped [render][particles]")
{
    const int dust = Particles::LookOf(Kind::Dust).count;
    CHECK(Particles::Count(Kind::Dust, 0.0f, 1.0f) == 0);
    CHECK(Particles::Count(Kind::Dust, 1.0f, 0.0f) == 0);
    CHECK(Particles::Count(Kind::Dust, 1.0f, Particles::Resolve(1, 70, true, false)) == dust);
    CHECK(Particles::Count(Kind::Dust, 1.0f, Particles::Resolve(1, 100, true, false)) ==
          static_cast<int>(std::round(static_cast<float>(dust) * 100.0f / 70.0f)));
    CHECK(Particles::Count(Kind::Fireflies, 0.5f, 1.0f) == Particles::LookOf(Kind::Fireflies).count / 2);
    CHECK(Particles::Count(Kind::Dust, 2.0f, 100.0f / 70.0f) == Particles::kMaxCount);
    CHECK(Particles::Count(Kind::None, 1.0f, 1.0f) == 0);
    CHECK(Particles::Count(Kind::Dust, -1.0f, 1.0f) == 0);
}

TEST_CASE("the phase runs 0..1 over the period and repeats [render][particles]")
{
    for (const double ms : {0.0, 1.0, 1000.0, 123456.0, kPeriodMs - 1.0, kPeriodMs, 5.0 * kPeriodMs + 777.0,
                            -1000.0, 86400000.0})
    {
        const float phase = Particles::Phase(ms);
        CHECK(phase >= 0.0f);
        CHECK(phase < 1.0f);
        CHECK(Particles::Phase(ms + kPeriodMs) == doctest::Approx(phase).epsilon(1e-5));
    }
    CHECK(Particles::Phase(kPeriodMs * 0.25) == doctest::Approx(0.25f));
}

TEST_CASE("turns are box lengths per period [render][particles]")
{
    CHECK(Particles::Turns(9.0f, 1800.0f) == doctest::Approx(9.0f * 2048.0f / 1800.0f));
    CHECK(Particles::Turns(-90.0f, 520.0f) == doctest::Approx(-90.0f * 2048.0f / 520.0f));
    CHECK(Particles::Turns(10.0f, 0.0f) == 0.0f);
}

TEST_CASE("a place wraps around the box and stays put while the hero walks [render][particles]")
{
    for (const float share : {-3.7f, -0.5f, -0.25f, 0.0f, 0.25f, 0.4999f, 0.5f, 1.25f, 10.6f})
    {
        const float wrapped = Particles::WrapShare(share);
        CHECK(wrapped >= -0.5f);
        CHECK(wrapped < 0.5f);
    }
    CHECK(Particles::WrapShare(0.5f) == doctest::Approx(-0.5f));
    CHECK(Particles::WrapShare(1.25f) == doctest::Approx(0.25f));

    // A lattice point at place 0.3 of the box, with the box centred at 1000: moving the centre less
    // than the point's distance to the box's far side keeps its world position.
    constexpr float box = Particles::kBoxSizeXY;
    const float before = WorldPosition(0.3f, 1000.0f, box);
    const float near = before - 1000.0f; // from the centre
    CHECK(std::abs(near) < box * 0.5f);
    for (const float move : {-200.0f, -50.0f, 10.0f, 100.0f, 300.0f})
    {
        // The point stays inside the moved box as long as it is less than half a box away.
        if (std::abs(near - move) < box * 0.5f - 1.0f)
        {
            CHECK(WorldPosition(0.3f, 1000.0f + move, box) == doctest::Approx(before).epsilon(1e-4));
        }
    }

    // A whole number of turns brings a place back where it started at phase 0 and 1.
    for (const float turns : {-7.0f, 0.0f, 3.0f, 11.0f})
    {
        CHECK(Particles::WrapShare(0.2f + turns * 1.0f) == doctest::Approx(Particles::WrapShare(0.2f)).epsilon(1e-4));
    }
}

TEST_CASE("the box's side fade is 1 inside 75 % and 0 at the side [render][particles]")
{
    CHECK(Particles::EdgeFade(0.0f, 0.0f) == 1.0f);
    CHECK(Particles::EdgeFade(0.3f, -0.2f) == 1.0f);
    CHECK(Particles::EdgeFade(0.375f, 0.0f) == doctest::Approx(1.0f));
    CHECK(Particles::EdgeFade(0.5f, 0.0f) == doctest::Approx(0.0f));
    CHECK(Particles::EdgeFade(0.1f, -0.5f) == doctest::Approx(0.0f));
    float previous = 1.0f;
    for (float x = 0.0f; x <= 0.5f; x += 0.01f)
    {
        const float fade = Particles::EdgeFade(x, 0.0f);
        CHECK(fade <= previous + 1e-6f);
        previous = fade;
    }
}

TEST_CASE("near the camera they fade in from 300 to 600 [render][particles]")
{
    CHECK(Particles::NearFade(0.0f) == 0.0f);
    CHECK(Particles::NearFade(300.0f) == 0.0f);
    CHECK(Particles::NearFade(450.0f) == doctest::Approx(0.5f));
    CHECK(Particles::NearFade(600.0f) == 1.0f);
    CHECK(Particles::NearFade(2000.0f) == 1.0f);
}

TEST_CASE("a radius on screen stays between the pixel limits, fainter below [render][particles]")
{
    const Particles::PixelSize small = Particles::ClampPixels(0.75f, 1.5f, 4.5f);
    CHECK(small.radius == 1.5f);
    CHECK(small.alphaScale == doctest::Approx(0.25f));
    const Particles::PixelSize none = Particles::ClampPixels(-1.0f, 1.5f, 4.5f);
    CHECK(none.radius == 1.5f);
    CHECK(none.alphaScale == 0.0f);
    const Particles::PixelSize big = Particles::ClampPixels(9.0f, 1.5f, 4.5f);
    CHECK(big.radius == 4.5f);
    CHECK(big.alphaScale == 1.0f);
    const Particles::PixelSize middle = Particles::ClampPixels(3.0f, 1.5f, 4.5f);
    CHECK(middle.radius == 3.0f);
    CHECK(middle.alphaScale == 1.0f);
}

TEST_CASE("the Classic outdoor gates of Lorencia and Devias [render][particles]")
{
    CHECK_FALSE(Particles::Outdoors(0, 4));
    CHECK(Particles::Outdoors(0, 0));
    CHECK_FALSE(Particles::Outdoors(2, 3));
    CHECK_FALSE(Particles::Outdoors(2, 10));
    CHECK(Particles::Outdoors(2, 1));
    CHECK(Particles::Outdoors(3, 4));
    CHECK(Particles::Outdoors(3, 10));
}

TEST_CASE("the presence snaps on a new map and otherwise eases towards its target [render][particles]")
{
    CHECK(Particles::SmoothPresence(0.0f, 1.0f, 0.016f, true) == 1.0f);
    CHECK(Particles::SmoothPresence(0.3f, 1.0f, -1.0f, false) == 1.0f);
    CHECK(Particles::SmoothPresence(0.3f, 0.3f, 0.5f, false) == doctest::Approx(0.3f));
    float presence = 0.0f;
    for (int frame = 0; frame < 120; ++frame)
    {
        const float next = Particles::SmoothPresence(presence, 1.0f, 1.0f / 60.0f, false);
        CHECK(next >= presence);
        CHECK(next <= 1.0f);
        presence = next;
    }
    CHECK(presence > 0.95f);
    // Leaving: about a second to fade out.
    presence = Particles::SmoothPresence(1.0f, 0.0f, 1.0f, false);
    CHECK(presence < 0.2f);
    CHECK(presence > 0.0f);
}

TEST_CASE("the drift keeps 35 % with the wind off [render][particles]")
{
    CHECK(Particles::WindShare(0.0f) == doctest::Approx(0.35f));
    CHECK(Particles::WindShare(1.0f) == doctest::Approx(1.0f));
    CHECK(Particles::WindShare(0.5f) == doctest::Approx(0.675f));
    CHECK(Particles::WindShare(-1.0f) == doctest::Approx(0.35f));
    CHECK(Particles::WindShare(2.0f) == doctest::Approx(1.0f));
}

TEST_CASE("every kind stays small and faint [render][particles]")
{
    CHECK(Particles::LookOf(Kind::None).count == 0);
    for (int index = 1; index < Particles::kKindCount; ++index)
    {
        const Particles::Look& look = Particles::LookOf(static_cast<Kind>(index));
        CAPTURE(index);
        CHECK(look.count > 0);
        CHECK(look.count <= Particles::kMaxCount);
        CHECK(look.minRadius <= look.maxRadius);
        CHECK(look.minPixels <= look.maxPixels);
        CHECK(look.zSize > 0.0f);
        CHECK(look.alpha <= 0.9f);
        if (!look.additive)
        {
            CHECK(look.alpha <= 0.6f);
        }
    }
}
