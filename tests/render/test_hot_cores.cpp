#include <doctest.h>

#include "Render/PostProcessing/HotCores.h"

#include <algorithm>
#include <array>

namespace
{
namespace HotCores = Render::PostProcessing::HotCores;

using Rgb = std::array<float, 3>;

Rgb Whiten(const Rgb& scene, const Rgb& effect, const float strength)
{
    const float weight = HotCores::CoreWeight(effect[0], effect[1], effect[2], strength);
    return {HotCores::Apply(scene[0], weight), HotCores::Apply(scene[1], weight), HotCores::Apply(scene[2], weight)};
}

// (max - mid) / (max - min): where the middle channel sits between the others, the hue's position.
float HueRatio(const Rgb& color)
{
    Rgb sorted = color;
    std::sort(sorted.begin(), sorted.end());
    return (sorted[2] - sorted[1]) / (sorted[2] - sorted[0]);
}
} // namespace

TEST_CASE("black has no core [render][hot_cores]")
{
    CHECK(HotCores::Heat(0.0f, 0.0f, 0.0f) == 0.0f);
    CHECK(HotCores::CoreWeight(0.0f, 0.0f, 0.0f, 1.0f) == 0.0f);
}

TEST_CASE("the core weight grows with the mean and with the brightest channel [render][hot_cores]")
{
    // The brightest channel held clipped, the mean rising.
    float previous = -1.0f;
    for (int step = 0; step <= 20; ++step)
    {
        const float other = static_cast<float>(step) / 20.0f;
        const float weight = HotCores::CoreWeight(1.0f, other, other, 1.0f);
        CHECK(weight >= previous);
        previous = weight;
    }
    CHECK(previous == doctest::Approx(1.0f));
    // The other channels held, the brightest one rising.
    previous = -1.0f;
    for (int step = 0; step <= 20; ++step)
    {
        const float peak = 0.8f + 0.2f * static_cast<float>(step) / 20.0f;
        const float weight = HotCores::CoreWeight(peak, 0.8f, 0.8f, 1.0f);
        CHECK(weight >= previous);
        previous = weight;
    }
    CHECK(previous > 0.0f);
}

TEST_CASE("dim or unclipped effects leave the scene alone [render][hot_cores]")
{
    const Rgb scene{0.3f, 0.2f, 0.1f};
    // The brightest channel at or below the threshold, however bright the mean.
    for (const Rgb& effect : {Rgb{0.85f, 0.85f, 0.85f}, Rgb{0.8f, 0.7f, 0.6f}, Rgb{0.5f, 0.85f, 0.2f}})
    {
        CHECK(HotCores::CoreWeight(effect[0], effect[1], effect[2], 1.0f) == 0.0f);
        CHECK(Whiten(scene, effect, 1.0f) == scene);
    }
    // The mean at or below the threshold, however clipped one channel is.
    for (const Rgb& effect : {Rgb{1.0f, 0.34f, 0.0f}, Rgb{1.0f, 0.2f, 0.1f}, Rgb{0.0f, 0.3f, 1.0f}})
    {
        CHECK(HotCores::CoreWeight(effect[0], effect[1], effect[2], 1.0f) == 0.0f);
        CHECK(Whiten(scene, effect, 1.0f) == scene);
    }
}

TEST_CASE("a pile of pure red never whitens [render][hot_cores]")
{
    const Rgb scene{0.9f, 0.1f, 0.05f};
    CHECK(HotCores::CoreWeight(1.0f, 0.0f, 0.0f, 1.0f) == 0.0f);
    CHECK(Whiten(scene, {1.0f, 0.0f, 0.0f}, 1.0f) == scene);
}

TEST_CASE("white stays white and a full weight gives white [render][hot_cores]")
{
    const Rgb white{1.0f, 1.0f, 1.0f};
    CHECK(HotCores::CoreWeight(1.0f, 1.0f, 1.0f, 1.0f) == doctest::Approx(1.0f));
    CHECK(Whiten(white, white, 1.0f) == white);
    CHECK(Whiten(white, {1.0f, 0.95f, 0.6f}, 0.5f) == white);
    for (float channel : {0.0f, 0.25f, 0.7f, 1.0f})
    {
        CHECK(HotCores::Apply(channel, 1.0f) == doctest::Approx(1.0f));
    }
}

TEST_CASE("whitening keeps the hue and the channel order [render][hot_cores]")
{
    const Rgb effect{1.0f, 0.9f, 0.55f}; // a yellow-white fire core
    const float weight = HotCores::CoreWeight(effect[0], effect[1], effect[2], 1.0f);
    REQUIRE(weight > 0.0f);
    REQUIRE(weight < 1.0f);
    for (const Rgb& scene : {Rgb{0.9f, 0.5f, 0.1f}, Rgb{0.2f, 0.4f, 0.8f}, Rgb{0.6f, 0.1f, 0.3f}})
    {
        const Rgb result = Whiten(scene, effect, 1.0f);
        CHECK(HueRatio(result) == doctest::Approx(HueRatio(scene)));
        for (int a = 0; a < 3; ++a)
        {
            CHECK(result[a] >= scene[a]);
            for (int b = 0; b < 3; ++b)
            {
                CHECK((scene[a] < scene[b]) == (result[a] < result[b]));
            }
        }
    }
}

TEST_CASE("strength 0 leaves every colour alone [render][hot_cores]")
{
    const Rgb scene{0.4f, 0.3f, 0.2f};
    for (const Rgb& effect : {Rgb{1.0f, 1.0f, 1.0f}, Rgb{1.0f, 0.9f, 0.55f}, Rgb{0.0f, 0.0f, 0.0f}})
    {
        CHECK(HotCores::CoreWeight(effect[0], effect[1], effect[2], 0.0f) == 0.0f);
        CHECK(Whiten(scene, effect, 0.0f) == scene);
    }
    // Strength scales the weight.
    CHECK(HotCores::CoreWeight(1.0f, 1.0f, 1.0f, 0.5f) == doctest::Approx(0.5f));
}
