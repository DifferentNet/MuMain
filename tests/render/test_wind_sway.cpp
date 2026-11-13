#include <doctest.h>

#include "Render/Models/WindSway.h"

#include <cmath>
#include <numbers>

namespace
{
namespace Wind = Render::Models::Wind;

constexpr float kFourPi = 4.0f * std::numbers::pi_v<float>;

// The shader's gust for a phase (skinned_textured.vert applyWind, travel and origin terms 0).
float Gust(float phase)
{
    return std::sin(phase) * 0.65f + std::sin(2.5f * phase) * 0.35f;
}
} // namespace

TEST_CASE("on at the saved strength [render][wind]")
{
    CHECK(Wind::Resolve(1, 100, true, false) == doctest::Approx(1.0f));
    CHECK(Wind::Resolve(1, 40, true, false) == doctest::Approx(0.4f));
}

TEST_CASE("the strength is clamped to 0..100 [render][wind]")
{
    CHECK(Wind::Resolve(1, 250, true, false) == doctest::Approx(1.0f));
    CHECK(Wind::Resolve(1, -5, true, false) == 0.0f);
}

TEST_CASE("off when not available, on Low, for 0 or unknown values, at strength 0 [render][wind]")
{
    CHECK(Wind::Resolve(1, 100, false, false) == 0.0f);
    CHECK(Wind::Resolve(1, 100, true, true) == 0.0f);
    CHECK(Wind::Resolve(0, 100, true, false) == 0.0f);
    CHECK(Wind::Resolve(2, 100, true, false) == 0.0f);
    CHECK(Wind::Resolve(1, 0, true, false) == 0.0f);
}

TEST_CASE("the Options row's reason, first that applies, locks it [render][wind]")
{
    using Block = Wind::Block;
    CHECK(Wind::ResolveBlock(true, false) == Block::None);
    CHECK(Wind::ResolveBlock(false, true) == Block::NoHdTextures);
    CHECK(Wind::ResolveBlock(true, true) == Block::LowPreset);
    CHECK_FALSE(Wind::Locks(Block::None));
    CHECK(Wind::Locks(Block::NoHdTextures));
    CHECK(Wind::Locks(Block::LowPreset));
}

TEST_CASE("foliage textures by name, any case and separator [render][wind]")
{
    using Kind = Wind::Kind;
    CHECK(Wind::ClassifyTexture("tree_04.tga") == Kind::Crown);
    CHECK(Wind::ClassifyTexture("TREE_04.TGA") == Kind::Crown);
    CHECK(Wind::ClassifyTexture("Data\\Object1\\tree_04.tga") == Kind::Crown);
    CHECK(Wind::ClassifyTexture("Data/Object8/IceTree0601.tga") == Kind::Crown);
    CHECK(Wind::ClassifyTexture("tree_01.tga") == Kind::Bush);
    CHECK(Wind::ClassifyTexture("cr1_grass03.tga") == Kind::Bush);
    CHECK(Wind::ClassifyTexture("Flower.tga") == Kind::Flower);
    CHECK(Wind::ClassifyTexture("sea3flower02.tga") == Kind::Flower);
}

TEST_CASE("solid parts, litter, hay, gift boxes and seaweed never sway [render][wind]")
{
    using Kind = Wind::Kind;
    CHECK(Wind::ClassifyTexture("tree_04.jpg") == Kind::None);
    CHECK(Wind::ClassifyTexture("tree_08.tga") == Kind::None);
    CHECK(Wind::ClassifyTexture("grass_01.tga") == Kind::None);
    CHECK(Wind::ClassifyTexture("leaf.tga") == Kind::None);
    CHECK(Wind::ClassifyTexture("bu01.tga") == Kind::None);
    CHECK(Wind::ClassifyTexture("tree_04") == Kind::None);
    CHECK(Wind::ClassifyTexture("tree_04.tgax") == Kind::None);
    CHECK(Wind::ClassifyTexture(".tga") == Kind::None);
    CHECK(Wind::ClassifyTexture("") == Kind::None);
    CHECK(Wind::ClassifyTexture("a_very_long_texture_name_that_is_no_foliage.tga") == Kind::None);
}

TEST_CASE("the phase stays in 0..4 pi and the gust is continuous at the wrap [render][wind]")
{
    for (const double ms : {0.0, 1.0, 1234.5, 7853.98, 60000.0, 3.6e6, 8.64e7})
    {
        const float phase = Wind::Phase(ms);
        CHECK(phase >= 0.0f);
        CHECK(phase < kFourPi);
    }
    CHECK(Wind::Phase(1000.0) == doctest::Approx(1.6f));

    // 4 pi / 1.6 per second: the time at which the phase wraps.
    const double wrapMs = 4.0 * std::numbers::pi / 1.6 * 1000.0;
    const float before = Gust(Wind::Phase(wrapMs - 0.5));
    const float after = Gust(Wind::Phase(wrapMs + 0.5));
    CHECK(std::fabs(before - after) < 0.01f);
}

TEST_CASE("the grass offset stays within 12 * strength and is 0 at strength 0 [render][wind]")
{
    for (int x = 0; x < 256; x += 17)
    {
        for (int y = 0; y < 256; y += 13)
        {
            for (const float phase : {0.0f, 1.0f, 5.5f, 12.0f})
            {
                CHECK(std::fabs(Wind::GrassOffset(phase, x, y, 1.0f)) <= 12.0f + 1e-4f);
                CHECK(std::fabs(Wind::GrassOffset(phase, x, y, 0.5f)) <= 6.0f + 1e-4f);
                CHECK(Wind::GrassOffset(phase, x, y, 0.0f) == 0.0f);
            }
        }
    }
}

TEST_CASE("the sway follows the kind, the object's scale and the strength [render][wind]")
{
    using Kind = Wind::Kind;
    const Wind::Sway crown = Wind::SwayFor(Kind::Crown, 1.0f, 1.0f);
    CHECK(crown.amplitude == doctest::Approx(6.0f));
    CHECK(crown.inverseHeightRange == doctest::Approx(1.0f / 350.0f));
    CHECK(Wind::SwayFor(Kind::Bush, 1.0f, 1.0f).amplitude == doctest::Approx(4.0f));
    CHECK(Wind::SwayFor(Kind::Flower, 1.0f, 1.0f).amplitude == doctest::Approx(3.0f));

    // A bigger tree grows over a taller range; its amplitude counts the scale only up to 1.5.
    const Wind::Sway big = Wind::SwayFor(Kind::Crown, 2.0f, 1.0f);
    CHECK(big.inverseHeightRange == doctest::Approx(1.0f / 700.0f));
    CHECK(big.amplitude == doctest::Approx(9.0f));
    CHECK(Wind::SwayFor(Kind::Crown, 0.2f, 1.0f).amplitude == doctest::Approx(3.0f));

    CHECK(Wind::SwayFor(Kind::Crown, 1.0f, 0.5f).amplitude == doctest::Approx(3.0f));
    CHECK(Wind::SwayFor(Kind::Crown, 1.0f, 0.0f).amplitude == 0.0f);
    CHECK(Wind::SwayFor(Kind::None, 1.0f, 1.0f).amplitude == 0.0f);
    CHECK(Wind::SwayFor(Kind::Crown, 0.0f, 1.0f).amplitude == 0.0f);
}
