#include <doctest.h>

#include "Render/PostProcessing/HazeSetting.h"

namespace
{
namespace Haze = Render::PostProcessing::Haze;

constexpr float kFogStart = 3000.0f;
constexpr float kHeroDistance = 1000.0f;
} // namespace

TEST_CASE("the strength is HazeStrength over 70, clamped to 0..100 [render][haze]")
{
    CHECK(Haze::Resolve(1, 70, true, false) == doctest::Approx(1.0f));
    CHECK(Haze::Resolve(1, 35, true, false) == doctest::Approx(0.5f));
    CHECK(Haze::Resolve(1, 100, true, false) == doctest::Approx(100.0f / 70.0f));
    CHECK(Haze::Resolve(1, 250, true, false) == doctest::Approx(100.0f / 70.0f));
    CHECK(Haze::Resolve(1, -5, true, false) == 0.0f);
}

TEST_CASE("off without HD textures, on Low, for 0 or unknown values, at strength 0 [render][haze]")
{
    CHECK(Haze::Resolve(1, 70, false, false) == 0.0f);
    CHECK(Haze::Resolve(1, 70, true, true) == 0.0f);
    CHECK(Haze::Resolve(0, 70, true, false) == 0.0f);
    CHECK(Haze::Resolve(2, 70, true, false) == 0.0f);
    CHECK(Haze::Resolve(1, 0, true, false) == 0.0f);
}

TEST_CASE("none at or before the start behind the hero [render][haze]")
{
    const Haze::Range range = Haze::ResolveRange(true, kHeroDistance, kFogStart);
    REQUIRE(range.valid);
    CHECK(range.start == doctest::Approx(kHeroDistance + Haze::kClearMargin));
    CHECK(range.end == doctest::Approx(kFogStart));
    CHECK(Haze::RangeShare(kHeroDistance, range) == 0.0f);
    CHECK(Haze::RangeShare(range.start, range) == 0.0f);
    CHECK(Haze::DistanceAmount(Haze::RangeShare(range.start, range), 0.3f) == 0.0f);
    CHECK(Haze::GroundAmount(-500.0f, 0.0f, 220.0f, 0.3f, 0.0f) == 0.0f);
}

TEST_CASE("the haze grows with distance up to the fog start [render][haze]")
{
    const Haze::Range range = Haze::ResolveRange(true, kHeroDistance, kFogStart);
    float previous = -1.0f;
    for (float distance = range.start; distance <= range.end + 200.0f; distance += 50.0f)
    {
        const float amount = Haze::DistanceAmount(Haze::RangeShare(distance, range), 0.3f);
        CHECK(amount >= previous);
        previous = amount;
    }
    CHECK(Haze::RangeShare(range.end, range) == doctest::Approx(1.0f));
    CHECK(Haze::DistanceAmount(1.0f, 0.3f) == doctest::Approx(0.3f));
}

TEST_CASE("never above the cap [render][haze]")
{
    CHECK(Haze::Combine(1.0f, 1.0f, 1.0f) == doctest::Approx(Haze::kMaxAmount));
    CHECK(Haze::Combine(0.5f * 100.0f / 70.0f, 0.5f * 100.0f / 70.0f, 1.0f) == doctest::Approx(Haze::kMaxAmount));
    CHECK(Haze::Combine(0.2f, 0.0f, 1.0f) == doctest::Approx(0.2f));
    CHECK(Haze::Combine(0.2f, 0.1f, 1.0f) == doctest::Approx(1.0f - 0.8f * 0.9f));
}

TEST_CASE("no ground fog above the layer, more the lower [render][haze]")
{
    constexpr float reference = 100.0f;
    constexpr float layer = 220.0f;
    CHECK(Haze::GroundAmount(reference + layer, reference, layer, 0.3f, 1.0f) == 0.0f);
    CHECK(Haze::GroundAmount(reference + layer + 50.0f, reference, layer, 0.3f, 1.0f) == 0.0f);
    CHECK(Haze::GroundAmount(reference, reference, layer, 0.3f, 1.0f) == doctest::Approx(0.3f));
    CHECK(Haze::GroundAmount(reference - 100.0f, reference, layer, 0.3f, 1.0f) == doctest::Approx(0.3f));
    CHECK(Haze::GroundAmount(reference + 110.0f, reference, layer, 0.3f, 1.0f) == doctest::Approx(0.3f * 0.25f));
    CHECK(Haze::GroundAmount(reference, reference, layer, 0.3f, 0.25f) == doctest::Approx(0.3f * 0.5f));
    CHECK(Haze::GroundAmount(reference, reference, 0.0f, 0.3f, 1.0f) == 0.0f);
}

TEST_CASE("it fades out to nothing at the fog's end [render][haze]")
{
    CHECK(Haze::FogFadeOut(2000.0f, 3000.0f, 4000.0f) == 1.0f);
    CHECK(Haze::FogFadeOut(3500.0f, 3000.0f, 4000.0f) == doctest::Approx(0.5f));
    CHECK(Haze::FogFadeOut(4000.0f, 3000.0f, 4000.0f) == 0.0f);
    CHECK(Haze::FogFadeOut(5000.0f, 3000.0f, 4000.0f) == 0.0f);
    CHECK(Haze::Combine(0.3f, 0.2f, Haze::FogFadeOut(4000.0f, 3000.0f, 4000.0f)) == 0.0f);
}

TEST_CASE("a range shorter than the minimum draws none [render][haze]")
{
    const Haze::Range range = Haze::ResolveRange(true, kFogStart - Haze::kClearMargin - 100.0f, kFogStart);
    CHECK_FALSE(range.valid);
    CHECK(Haze::RangeShare(kFogStart, range) == 0.0f);
    CHECK_FALSE(Haze::ResolveRange(true, kFogStart, kFogStart).valid);
}

TEST_CASE("without a hero the haze starts at a share of the fog start [render][haze]")
{
    const Haze::Range range = Haze::ResolveRange(false, 0.0f, kFogStart);
    REQUIRE(range.valid);
    CHECK(range.start == doctest::Approx(kFogStart * Haze::kNoHeroStartShare));
    CHECK(range.end == doctest::Approx(kFogStart));
}

TEST_CASE("the ground fog's reference height follows smoothly and snaps on a jump [render][haze]")
{
    CHECK(Haze::SmoothReference(0.0f, 100.0f, 0.0f, false) == doctest::Approx(0.0f));
    const float oneSecond = Haze::SmoothReference(0.0f, 100.0f, Haze::kReferenceSeconds, false);
    CHECK(oneSecond == doctest::Approx(100.0f * (1.0f - std::exp(-1.0f))));
    CHECK(Haze::SmoothReference(0.0f, 100.0f, 60.0f, false) == doctest::Approx(100.0f).epsilon(0.001));
    CHECK(Haze::SmoothReference(0.0f, 100.0f, 0.016f, true) == 100.0f);
    CHECK(Haze::SmoothReference(0.0f, Haze::kReferenceSnap + 1.0f, 0.016f, false) == Haze::kReferenceSnap + 1.0f);
}
