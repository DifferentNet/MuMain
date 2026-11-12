#include <doctest.h>

#include "Render/PostProcessing/SsaoSetting.h"

namespace
{
namespace Ssao = Render::PostProcessing::Ssao;
} // namespace

TEST_CASE("on at the saved strength [render][ssao]")
{
    CHECK(Ssao::Resolve(1, 100, true, false) == doctest::Approx(1.0f));
    CHECK(Ssao::Resolve(1, 40, true, false) == doctest::Approx(0.4f));
}

TEST_CASE("the strength is clamped to 0..100 [render][ssao]")
{
    CHECK(Ssao::Resolve(1, 250, true, false) == doctest::Approx(1.0f));
    CHECK(Ssao::Resolve(1, -5, true, false) == 0.0f);
}

TEST_CASE("off when not available, on Low, for 0 or unknown values, at strength 0 [render][ssao]")
{
    CHECK(Ssao::Resolve(1, 100, false, false) == 0.0f);
    CHECK(Ssao::Resolve(1, 100, true, true) == 0.0f);
    CHECK(Ssao::Resolve(0, 100, true, false) == 0.0f);
    CHECK(Ssao::Resolve(2, 100, true, false) == 0.0f);
    CHECK(Ssao::Resolve(1, 0, true, false) == 0.0f);
}

TEST_CASE("the linear depth inverts a zero-to-one perspective projection [render][ssao]")
{
    constexpr float nearPlane = 20.0f;
    constexpr float farPlane = 5000.0f;
    // glm::perspective with GLM_FORCE_DEPTH_ZERO_TO_ONE (right-handed): proj[2][2], proj[3][2].
    const float p22 = farPlane / (nearPlane - farPlane);
    const float p32 = -(farPlane * nearPlane) / (farPlane - nearPlane);
    CHECK(Ssao::LinearDepth(0.0f, p22, p32) == doctest::Approx(nearPlane));
    CHECK(Ssao::LinearDepth(1.0f, p22, p32) == doctest::Approx(farPlane));

    // The view point at z = -distance: clip z = p22 * z + p32, clip w = -z.
    constexpr float distance = 1234.0f;
    const float depth = (p22 * -distance + p32) / distance;
    CHECK(Ssao::LinearDepth(depth, p22, p32) == doctest::Approx(distance).epsilon(0.001));
}

TEST_CASE("the fade follows the fog, never past the far plane [render][ssao]")
{
    const Ssao::FadeRange fog = Ssao::ResolveFade(true, 1500.0f, 3000.0f, 5000.0f);
    CHECK(fog.start == doctest::Approx(1500.0f));
    CHECK(fog.end == doctest::Approx(3000.0f));

    const Ssao::FadeRange clamped = Ssao::ResolveFade(true, 1500.0f, 9000.0f, 5000.0f);
    CHECK(clamped.end == doctest::Approx(5000.0f));
}

TEST_CASE("without fog or with a broken range the fade covers the view's last part [render][ssao]")
{
    for (const Ssao::FadeRange fade :
         {Ssao::ResolveFade(false, 1500.0f, 3000.0f, 5000.0f), Ssao::ResolveFade(true, 3000.0f, 1500.0f, 5000.0f),
          Ssao::ResolveFade(true, 6000.0f, 8000.0f, 5000.0f)})
    {
        CHECK(fade.start == doctest::Approx(5000.0f * Ssao::kNoFogFadeStart));
        CHECK(fade.end == doctest::Approx(5000.0f));
    }
}
