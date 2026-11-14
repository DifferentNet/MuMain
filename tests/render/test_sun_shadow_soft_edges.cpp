#include <doctest.h>

#include "Render/Lighting/SunShadowSoftEdges.h"
#include "Render/Renderer/SunShadow.h"

#include <cstddef>
#include <cstring>

namespace
{
namespace SunShadowSoftEdges = Render::Lighting::SunShadowSoftEdges;
using doctest::Approx;
using SunShadowSoftEdges::Block;

// The bytes of the uniform before the widening edge's part (what the fixed edge has always used).
constexpr std::size_t kFixedEdgeBytes = offsetof(mu::SunShadowUniform, penumbra);

// A frame as SunShadow::BuildFrame gives one: the box of SunShadowSetting.cpp, a default mood's edge.
mu::SunShadowFrame TestFrame(std::uint32_t mapSize)
{
    mu::SunShadowFrame frame{};
    frame.enabled = true;
    frame.towardSun = {0.5f, -0.5f, 0.5f};
    frame.focus = {12800.0f, 12800.0f, 300.0f};
    frame.halfExtent = 2000.0f;
    frame.depthRange = 6000.0f;
    frame.softness = 4.0f;
    frame.mapSize = mapSize;
    return frame;
}

float TexelWorld(const mu::SunShadowFrame& frame)
{
    return 2.0f * frame.halfExtent / static_cast<float>(frame.mapSize);
}

bool SameFixedEdgeBytes(const mu::SunShadowUniform& a, const mu::SunShadowUniform& b)
{
    return std::memcmp(&a, &b, kFixedEdgeBytes) == 0;
}

bool PenumbraIsZero(const mu::SunShadowUniform& uniform)
{
    return uniform.penumbra[0] == 0.0f && uniform.penumbra[1] == 0.0f && uniform.penumbra[2] == 0.0f &&
           uniform.penumbra[3] == 0.0f;
}
} // namespace

TEST_CASE("the strength is SunShadowSoftEdgesStrength over 100, clamped, and off when not available [render][sunshadow]")
{
    CHECK(SunShadowSoftEdges::Resolve(1, 70, true, false) == Approx(0.7f));
    CHECK(SunShadowSoftEdges::Resolve(1, 100, true, false) == Approx(1.0f));
    CHECK(SunShadowSoftEdges::Resolve(1, 250, true, false) == Approx(1.0f));
    CHECK(SunShadowSoftEdges::Resolve(1, -5, true, false) == 0.0f);
    CHECK(SunShadowSoftEdges::Resolve(1, 0, true, false) == 0.0f);
    CHECK(SunShadowSoftEdges::Resolve(0, 70, true, false) == 0.0f);
    CHECK(SunShadowSoftEdges::Resolve(2, 70, true, false) == 0.0f);
    CHECK(SunShadowSoftEdges::Resolve(1, 70, false, false) == 0.0f);
    CHECK(SunShadowSoftEdges::Resolve(1, 70, true, true) == 0.0f);
    CHECK(SunShadowSoftEdges::kDefaultPercent == 70);
}

TEST_CASE("the row is locked by the first reason that applies: HD textures, Low, sun shadows [render][sunshadow]")
{
    CHECK(SunShadowSoftEdges::ResolveBlock(false, true, false) == Block::NoHdTextures);
    CHECK(SunShadowSoftEdges::ResolveBlock(false, false, true) == Block::NoHdTextures);
    CHECK(SunShadowSoftEdges::ResolveBlock(true, true, false) == Block::LowPreset);
    CHECK(SunShadowSoftEdges::ResolveBlock(true, true, true) == Block::LowPreset);
    CHECK(SunShadowSoftEdges::ResolveBlock(true, false, false) == Block::NoSunShadows);
    CHECK(SunShadowSoftEdges::ResolveBlock(true, false, true) == Block::None);
    CHECK(SunShadowSoftEdges::Locks(Block::NoHdTextures));
    CHECK(SunShadowSoftEdges::Locks(Block::LowPreset));
    CHECK(SunShadowSoftEdges::Locks(Block::NoSunShadows));
    CHECK_FALSE(SunShadowSoftEdges::Locks(Block::None));
}

TEST_CASE("the edge's growth and cap follow the strength, clamped to 0..1 [render][sunshadow]")
{
    CHECK(SunShadowSoftEdges::Slope(0.0f) == 0.0f);
    CHECK(SunShadowSoftEdges::GrowthWorld(0.0f) == 0.0f);
    CHECK(SunShadowSoftEdges::Slope(0.7f) == Approx(0.042f));
    CHECK(SunShadowSoftEdges::GrowthWorld(0.7f) == Approx(28.0f));
    CHECK(SunShadowSoftEdges::Slope(1.0f) == Approx(0.06f));
    CHECK(SunShadowSoftEdges::GrowthWorld(1.0f) == Approx(40.0f));
    CHECK(SunShadowSoftEdges::Slope(2.5f) == Approx(0.06f));
    CHECK(SunShadowSoftEdges::GrowthWorld(2.5f) == Approx(40.0f));
    CHECK(SunShadowSoftEdges::Slope(-1.0f) == 0.0f);
    CHECK(SunShadowSoftEdges::GrowthWorld(-1.0f) == 0.0f);
}

TEST_CASE("strength 0 leaves the frame as it is [render][sunshadow]")
{
    mu::SunShadowFrame frame{};
    SunShadowSoftEdges::ApplyToFrame(frame, 0.0f);
    CHECK(frame.penumbraSlope == 0.0f);
    CHECK(frame.penumbraGrowth == 0.0f);
    SunShadowSoftEdges::ApplyToFrame(frame, -0.5f);
    CHECK(frame.penumbraSlope == 0.0f);
    CHECK(frame.penumbraGrowth == 0.0f);

    const mu::SunShadowFrame reference{};
    CHECK(frame.enabled == reference.enabled);
    CHECK(frame.softness == reference.softness);
    CHECK(frame.mapSize == reference.mapSize);
    CHECK(mu::PackSunShadowUniform(frame).penumbra[0] == 0.0f);
}

TEST_CASE("off packs no widening edge and the same fixed edge bytes as on [render][sunshadow]")
{
    const mu::SunShadowFrame off = TestFrame(mu::kSunShadowMapMedium);
    mu::SunShadowFrame on = off;
    SunShadowSoftEdges::ApplyToFrame(on, 0.7f);
    const mu::SunShadowUniform offUniform = mu::PackSunShadowUniform(off);
    const mu::SunShadowUniform onUniform = mu::PackSunShadowUniform(on);
    CHECK(PenumbraIsZero(offUniform));
    CHECK_FALSE(PenumbraIsZero(onUniform));
    CHECK(SameFixedEdgeBytes(offUniform, onUniform));
}

TEST_CASE("on at Medium packs the growth, the widest edge, the search radius and the blocker bias [render][sunshadow]")
{
    mu::SunShadowFrame frame = TestFrame(mu::kSunShadowMapMedium);
    SunShadowSoftEdges::ApplyToFrame(frame, 0.7f);
    const mu::SunShadowUniform uniform = mu::PackSunShadowUniform(frame);
    const float texelWorld = TexelWorld(frame); // 4000 / 2048
    CHECK(uniform.penumbra[0] == Approx(0.042f * 6000.0f / texelWorld)); // 129.024
    CHECK(uniform.penumbra[1] == Approx((4.0f + 28.0f) / texelWorld));   // 16.384
    CHECK(uniform.penumbra[2] == Approx(uniform.penumbra[1]));
    CHECK(uniform.penumbra[3] == Approx(8.0f / 6000.0f));
}

TEST_CASE("at High the edge has twice the texels, never below the fixed edge, never above 48 [render][sunshadow]")
{
    mu::SunShadowFrame medium = TestFrame(mu::kSunShadowMapMedium);
    mu::SunShadowFrame high = TestFrame(mu::kSunShadowMapHigh);
    SunShadowSoftEdges::ApplyToFrame(medium, 0.7f);
    SunShadowSoftEdges::ApplyToFrame(high, 0.7f);
    const mu::SunShadowUniform mediumUniform = mu::PackSunShadowUniform(medium);
    const mu::SunShadowUniform highUniform = mu::PackSunShadowUniform(high);
    CHECK(highUniform.penumbra[1] == Approx(2.0f * mediumUniform.penumbra[1]));
    CHECK(highUniform.penumbra[0] == Approx(2.0f * mediumUniform.penumbra[0]));
    CHECK(highUniform.penumbra[3] == Approx(mediumUniform.penumbra[3]));

    // A map edge under half a texel: the fixed edge is half a texel, the widest edge no narrower.
    mu::SunShadowFrame thin = TestFrame(mu::kSunShadowMapMedium);
    thin.softness = 0.1f;
    thin.penumbraSlope = 0.01f;
    thin.penumbraGrowth = 0.0f;
    const mu::SunShadowUniform thinUniform = mu::PackSunShadowUniform(thin);
    CHECK(thinUniform.directionAndSoftness[3] == Approx(0.5f));
    CHECK(thinUniform.penumbra[1] == Approx(thinUniform.directionAndSoftness[3]));

    // The widest a map and the full strength can ask for is cut at 48 texels.
    mu::SunShadowFrame wide = TestFrame(mu::kSunShadowMapHigh);
    wide.softness = 40.0f;
    SunShadowSoftEdges::ApplyToFrame(wide, 1.0f);
    const mu::SunShadowUniform wideUniform = mu::PackSunShadowUniform(wide);
    CHECK(wideUniform.penumbra[1] == Approx(48.0f));
    CHECK(wideUniform.penumbra[2] == Approx(48.0f));
}

TEST_CASE("a disabled frame packs all zeros [render][sunshadow]")
{
    mu::SunShadowFrame frame = TestFrame(mu::kSunShadowMapMedium);
    SunShadowSoftEdges::ApplyToFrame(frame, 0.7f);
    frame.enabled = false;
    const mu::SunShadowUniform uniform = mu::PackSunShadowUniform(frame);
    const mu::SunShadowUniform zero{};
    CHECK(std::memcmp(&uniform, &zero, sizeof(mu::SunShadowUniform)) == 0);

    mu::SunShadowFrame noMap = TestFrame(0u);
    SunShadowSoftEdges::ApplyToFrame(noMap, 0.7f);
    const mu::SunShadowUniform noMapUniform = mu::PackSunShadowUniform(noMap);
    CHECK(std::memcmp(&noMapUniform, &zero, sizeof(mu::SunShadowUniform)) == 0);
}

TEST_CASE("the widening edge follows the fixed edge in the uniform block [render][sunshadow]")
{
    CHECK(offsetof(mu::SunShadowUniform, penumbra) == 96);
    CHECK(sizeof(mu::SunShadowUniform) == 112);
}
