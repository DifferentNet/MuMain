#include <doctest.h>

#include "Render/Terrain/TerrainDetail.h"

namespace
{
namespace Detail = Render::Terrain::Detail;

constexpr int SelfDetail = static_cast<int>(Detail::Variant::SelfDetail);
constexpr int Noise = static_cast<int>(Detail::Variant::Noise);
} // namespace

TEST_CASE("the self-detail variant has a positive scale [render][terrain_detail]")
{
    const Detail::Params params = Detail::Resolve(SelfDetail, 60, true);
    CHECK(params.strength == doctest::Approx(0.6f));
    CHECK(params.scale == doctest::Approx(Detail::kSelfDetailScale));
}

TEST_CASE("the noise variant has a negative scale [render][terrain_detail]")
{
    const Detail::Params params = Detail::Resolve(Noise, 100, true);
    CHECK(params.strength == doctest::Approx(1.0f));
    CHECK(params.scale == doctest::Approx(-Detail::kNoiseScale));
}

TEST_CASE("off without HD textures, for variant 0 or unknown, and at strength 0 [render][terrain_detail]")
{
    CHECK(Detail::Resolve(SelfDetail, 60, false).strength == 0.0f);
    CHECK(Detail::Resolve(0, 60, true).strength == 0.0f);
    CHECK(Detail::Resolve(3, 60, true).strength == 0.0f);
    CHECK(Detail::Resolve(-1, 60, true).strength == 0.0f);
    CHECK(Detail::Resolve(Noise, 0, true).strength == 0.0f);
    CHECK(Detail::Resolve(Noise, -20, true).strength == 0.0f);
}

TEST_CASE("the strength is clamped to 100 percent [render][terrain_detail]")
{
    CHECK(Detail::Resolve(SelfDetail, 250, true).strength == doctest::Approx(1.0f));
}
