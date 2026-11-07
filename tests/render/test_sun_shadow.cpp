#include "doctest.h"

#include "Data/GameData/MapData/LightMood.h"
#include "Render/Lighting/SunShadowSetting.h"
#include "Render/Renderer/SunShadow.h"

#include <array>
#include <cmath>

namespace
{
using Render::Lighting::SunShadow::BuildFrame;

// A world point through the sun's view-projection (column-major), as the shaders apply it.
std::array<float, 4> Project(const std::array<float, 16>& m, float x, float y, float z)
{
    std::array<float, 4> clip{};
    for (int row = 0; row < 4; ++row)
    {
        clip[static_cast<std::size_t>(row)] = m[static_cast<std::size_t>(row)] * x + m[static_cast<std::size_t>(4 + row)] * y +
                                              m[static_cast<std::size_t>(8 + row)] * z + m[static_cast<std::size_t>(12 + row)];
    }
    return clip;
}

mu::SunShadowFrame TestFrame()
{
    mu::SunShadowFrame frame{};
    frame.enabled = true;
    frame.towardSun = {0.5f, -0.5f, 0.5f};
    frame.focus = {12800.0f, 12800.0f, 300.0f};
    frame.halfExtent = 2000.0f;
    frame.depthRange = 6000.0f;
    frame.mapSize = mu::kSunShadowMapMedium;
    return frame;
}
} // namespace

TEST_CASE("the Options level picks the shadow map size [render][sunshadow]")
{
    CHECK(mu::SunShadowMapSize(0) == 0u);
    CHECK(mu::SunShadowMapSize(1) == 2048u);
    CHECK(mu::SunShadowMapSize(2) == 4096u);
    CHECK(mu::SunShadowMapSize(3) == 0u);
    CHECK(mu::SunShadowMapSize(-1) == 0u);
}

TEST_CASE("the focus lands in the middle of the shadow map, half way in depth [render][sunshadow]")
{
    const mu::SunShadowFrame frame = TestFrame();
    const auto clip = Project(mu::SunShadowViewProjection(frame), frame.focus[0], frame.focus[1], frame.focus[2]);
    const float texel = 2.0f / static_cast<float>(frame.mapSize);
    // Texel snapping moves the box by less than one texel.
    CHECK(std::abs(clip[0]) <= texel);
    CHECK(std::abs(clip[1]) <= texel);
    CHECK(clip[2] == doctest::Approx(0.5f).epsilon(0.001));
    CHECK(clip[3] == doctest::Approx(1.0f));
}

TEST_CASE("a point nearer the sun has a smaller depth [render][sunshadow]")
{
    const mu::SunShadowFrame frame = TestFrame();
    const auto matrix = mu::SunShadowViewProjection(frame);
    const auto ground = Project(matrix, frame.focus[0], frame.focus[1], frame.focus[2]);
    const auto above = Project(matrix, frame.focus[0], frame.focus[1], frame.focus[2] + 500.0f);
    CHECK(above[2] < ground[2]);
}

TEST_CASE("a point half the box away reaches the map edge [render][sunshadow]")
{
    // The sun straight overhead: world X and Y map to the shadow map's axes.
    mu::SunShadowFrame frame = TestFrame();
    frame.towardSun = {0.0f, 0.0f, 1.0f};
    const auto matrix = mu::SunShadowViewProjection(frame);
    const auto edge = Project(matrix, frame.focus[0] + frame.halfExtent, frame.focus[1], frame.focus[2]);
    const auto other = Project(matrix, frame.focus[0], frame.focus[1] + frame.halfExtent, frame.focus[2]);
    CHECK(std::max(std::abs(edge[0]), std::abs(edge[1])) == doctest::Approx(1.0f).epsilon(0.002));
    CHECK(std::max(std::abs(other[0]), std::abs(other[1])) == doctest::Approx(1.0f).epsilon(0.002));
}

TEST_CASE("the uniform carries the strength, the texel and a unit sun direction [render][sunshadow]")
{
    mu::SunShadowFrame frame = TestFrame();
    frame.strength = 1.5f;
    frame.softness = 20.0f;
    const mu::SunShadowUniform uniform = mu::PackSunShadowUniform(frame);
    CHECK(uniform.parameters[0] == doctest::Approx(1.0f / 2048.0f));
    CHECK(uniform.parameters[1] == doctest::Approx(1.0f));
    CHECK(uniform.parameters[3] == doctest::Approx(1.0f));
    CHECK(uniform.directionAndSoftness[3] == doctest::Approx(8.0f));
    const float length = std::sqrt(uniform.directionAndSoftness[0] * uniform.directionAndSoftness[0] +
                                   uniform.directionAndSoftness[1] * uniform.directionAndSoftness[1] +
                                   uniform.directionAndSoftness[2] * uniform.directionAndSoftness[2]);
    CHECK(length == doctest::Approx(1.0f));

    frame.enabled = false;
    CHECK(mu::PackSunShadowUniform(frame).parameters[3] == 0.0f);
}

TEST_CASE("a sun near the horizon is raised so its shadows stay in the box [render][sunshadow]")
{
    mu::SunShadowFrame frame = TestFrame();
    frame.towardSun = {1.0f, 0.0f, 0.0f};
    const mu::SunShadowUniform uniform = mu::PackSunShadowUniform(frame);
    // z is raised to 0.3 before the direction is made unit length again (about 17 degrees).
    CHECK(uniform.directionAndSoftness[2] == doctest::Approx(0.3f / std::sqrt(1.09f)));
}

TEST_CASE("a map's mood gives the frame its sun [render][sunshadow]")
{
    Data::Maps::LightMood mood{};
    const float focus[3] = {100.0f, 200.0f, 300.0f};

    // The default sun stands where the Classic terrain light comes from.
    const mu::SunShadowFrame frame = BuildFrame(mood, 1, focus);
    CHECK(frame.enabled);
    CHECK(frame.mapSize == 2048u);
    CHECK(frame.towardSun[0] > 0.0f);
    CHECK(frame.towardSun[1] < 0.0f);
    CHECK(frame.towardSun[2] == doctest::Approx(std::sin(40.0 * 3.14159265358979323846 / 180.0)));
    CHECK(frame.strength == doctest::Approx(0.45f));
    CHECK(frame.focus[1] == 200.0f);

    CHECK(BuildFrame(mood, 2, focus).mapSize == 4096u);
    CHECK_FALSE(BuildFrame(mood, 0, focus).enabled);

    mood.sunShadow = 0.0;
    CHECK_FALSE(BuildFrame(mood, 2, focus).enabled);
}
