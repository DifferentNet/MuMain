#include <doctest.h>

#include "Render/Effects/SoftParticles.h"
#include "Render/Renderer/SceneLighting.h"

#include <cstddef>

namespace
{
namespace Soft = Render::Effects::SoftParticles;
using doctest::Approx;
} // namespace

TEST_CASE("the softness follows the sprite's smaller half size, within limits [render][soft_particles]")
{
    // 0.35 of the smaller half size, between 8 and 40.
    CHECK(Soft::kSoftnessScale == Approx(0.35f));
    CHECK(Soft::kMinSoftness == Approx(8.0f));
    CHECK(Soft::kMaxSoftness == Approx(40.0f));
    CHECK(Soft::Softness(60.0f, 80.0f) == Approx(21.0f));
    CHECK(Soft::Softness(80.0f, 60.0f) == Approx(21.0f));
    CHECK(Soft::Softness(1.0f, 1.0f) == Approx(Soft::kMinSoftness));
    CHECK(Soft::Softness(0.0f, 500.0f) == Approx(Soft::kMinSoftness));
    CHECK(Soft::Softness(800.0f, 900.0f) == Approx(Soft::kMaxSoftness));
    CHECK(Soft::Softness(120.0f, 120.0f) == Approx(Soft::kMaxSoftness));
}

TEST_CASE("a sprite fades to nothing where it touches the world behind it [render][soft_particles]")
{
    // The world 500 away, softness 40.
    CHECK(Soft::Fade(500.0f, 500.0f, 40.0f) == Approx(0.0f));
    CHECK(Soft::Fade(500.0f, 520.0f, 40.0f) == Approx(0.0f)); // behind the world (the depth test hides it)
    CHECK(Soft::Fade(500.0f, 480.0f, 40.0f) == Approx(0.5f));
    CHECK(Soft::Fade(500.0f, 460.0f, 40.0f) == Approx(1.0f));
    CHECK(Soft::Fade(500.0f, 100.0f, 40.0f) == Approx(1.0f));
}

TEST_CASE("without depth or softness a sprite stays whole [render][soft_particles]")
{
    CHECK(Soft::Fade(-1.0f, 480.0f, 40.0f) == Approx(1.0f));
    CHECK(Soft::Fade(500.0f, 499.0f, 0.0f) == Approx(1.0f));
    CHECK(Soft::Fade(500.0f, 499.0f, -5.0f) == Approx(1.0f));
}

TEST_CASE("only blended, depth-tested, not depth-writing draws have a soft twin [render][soft_particles]")
{
    using mu::BlendMode;
    CHECK(Soft::TwinFor(true, BlendMode::Alpha, true, false) == Soft::Twin::Alpha);
    CHECK(Soft::TwinFor(true, BlendMode::Additive, true, false) == Soft::Twin::Glow);
    CHECK(Soft::TwinFor(true, BlendMode::Glow, true, false) == Soft::Twin::Glow);
    CHECK(Soft::TwinFor(true, BlendMode::Subtract, true, false) == Soft::Twin::None);
    CHECK(Soft::TwinFor(true, BlendMode::Mixed, true, false) == Soft::Twin::None);
    CHECK(Soft::TwinFor(false, BlendMode::Alpha, true, false) == Soft::Twin::None);
    CHECK(Soft::TwinFor(true, BlendMode::Alpha, false, false) == Soft::Twin::None);
    CHECK(Soft::TwinFor(true, BlendMode::Glow, true, true) == Soft::Twin::None);
}

TEST_CASE("the soft block is two registers, all zero (off) by default [render][soft_particles]")
{
    CHECK(sizeof(mu::SoftParticlesUniform) == 32);
    CHECK(offsetof(mu::SoftParticlesUniform, depth) == 16);
    const mu::SoftParticlesUniform uniform{};
    for (int i = 0; i < 4; ++i)
    {
        CHECK(uniform.projection[i] == 0.0f);
        CHECK(uniform.depth[i] == 0.0f);
    }
}
