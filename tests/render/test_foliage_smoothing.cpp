#include <doctest.h>

#include "Render/Textures/FoliageSmoothing.h"
#include "Render/Textures/TextureFiltering.h"

#include <optional>

namespace
{
namespace FoliageSmoothing = Render::Textures::FoliageSmoothing;
using doctest::Approx;
using FoliageSmoothing::Block;
} // namespace

TEST_CASE("the strength is FoliageSmoothingStrength over 100, clamped, and off when not available [render][foliage]")
{
    CHECK(FoliageSmoothing::Resolve(1, 70, true, false) == Approx(0.7f));
    CHECK(FoliageSmoothing::Resolve(1, 100, true, false) == Approx(1.0f));
    CHECK(FoliageSmoothing::Resolve(1, 250, true, false) == Approx(1.0f));
    CHECK(FoliageSmoothing::Resolve(1, -5, true, false) == 0.0f);
    CHECK(FoliageSmoothing::Resolve(1, 0, true, false) == 0.0f);
    CHECK(FoliageSmoothing::Resolve(0, 70, true, false) == 0.0f);
    CHECK(FoliageSmoothing::Resolve(2, 70, true, false) == 0.0f);
    CHECK(FoliageSmoothing::Resolve(1, 70, false, false) == 0.0f);
    CHECK(FoliageSmoothing::Resolve(1, 70, true, true) == 0.0f);
    CHECK(FoliageSmoothing::kDefaultPercent == 70);
}

TEST_CASE("the row is locked by the first reason that applies [render][foliage]")
{
    CHECK(FoliageSmoothing::ResolveBlock(false, true) == Block::NoHdTextures);
    CHECK(FoliageSmoothing::ResolveBlock(false, false) == Block::NoHdTextures);
    CHECK(FoliageSmoothing::ResolveBlock(true, true) == Block::LowPreset);
    CHECK(FoliageSmoothing::ResolveBlock(true, false) == Block::None);
    CHECK(FoliageSmoothing::Locks(Block::NoHdTextures));
    CHECK(FoliageSmoothing::Locks(Block::LowPreset));
    CHECK_FALSE(FoliageSmoothing::Locks(Block::None));
}

TEST_CASE("the cut-out mip bias moves the HD bias toward the softer mip with the strength [render][foliage]")
{
    const float hdBias = Render::Textures::MipLodBias(true);
    REQUIRE(hdBias == Approx(-0.5f));
    CHECK_FALSE(FoliageSmoothing::CutoutMipLodBias(hdBias, 0.0f).has_value());
    CHECK_FALSE(FoliageSmoothing::CutoutMipLodBias(hdBias, -0.1f).has_value());

    const std::optional<float> half = FoliageSmoothing::CutoutMipLodBias(hdBias, 0.5f);
    const std::optional<float> standard = FoliageSmoothing::CutoutMipLodBias(hdBias, 0.7f);
    const std::optional<float> full = FoliageSmoothing::CutoutMipLodBias(hdBias, 1.0f);
    REQUIRE(half.has_value());
    REQUIRE(standard.has_value());
    REQUIRE(full.has_value());
    CHECK(*half == Approx(0.0f));
    CHECK(*standard == Approx(0.2f));
    CHECK(*full == Approx(0.5f));

    const std::optional<float> noHd = FoliageSmoothing::CutoutMipLodBias(0.0f, 0.7f);
    REQUIRE(noHd.has_value());
    CHECK(*noHd == Approx(0.7f));
}

TEST_CASE("a texture is a cut-out when some of its texels fail the alpha test [render][foliage]")
{
    CHECK_FALSE(FoliageSmoothing::IsCutout(1.0f));
    CHECK_FALSE(FoliageSmoothing::IsCutout(FoliageSmoothing::kCutoutCoverageMax));
    CHECK_FALSE(FoliageSmoothing::IsCutout(0.95f));
    CHECK(FoliageSmoothing::IsCutout(0.85f));
    CHECK(FoliageSmoothing::IsCutout(0.4f));
    CHECK(FoliageSmoothing::IsCutout(0.0f));
}
