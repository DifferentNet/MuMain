#include <doctest.h>

#include "Render/Terrain/TerrainVariation.h"

#include <cmath>

namespace Variation = Render::Terrain::Variation;

TEST_CASE("Terrain variation: settings to params")
{
    CHECK_FALSE(Variation::Resolve(2, 100, false).macro);
    CHECK_FALSE(Variation::Resolve(0, 100, true).macro);
    CHECK_FALSE(Variation::Resolve(2, 0, true).macro);
    CHECK_FALSE(Variation::Resolve(7, 100, true).macro);

    const auto macro = Variation::Resolve(1, 50, true);
    CHECK(macro.macro);
    CHECK_FALSE(macro.flips);
    CHECK(macro.strength == doctest::Approx(0.5f));

    const auto both = Variation::Resolve(2, 250, true);
    CHECK(both.macro);
    CHECK(both.flips);
    CHECK(both.strength == doctest::Approx(1.0f));
}

TEST_CASE("Terrain variation: macro factor stays within its swings and varies")
{
    float factor[3];
    Variation::MacroFactor(10, 20, 0, 0.0f, factor);
    CHECK(factor[0] == doctest::Approx(1.0f));
    CHECK(factor[1] == doctest::Approx(1.0f));
    CHECK(factor[2] == doctest::Approx(1.0f));

    float lowest = 2.0f;
    float highest = 0.0f;
    for (int y = 0; y < 256; y += 3)
    {
        for (int x = 0; x < 256; x += 3)
        {
            Variation::MacroFactor(x, y, 1, 1.0f, factor);
            lowest = std::fmin(lowest, factor[1]);
            highest = std::fmax(highest, factor[1]);
            const float limit = (1.0f + Variation::kBrightnessSwing) * (1.0f + Variation::kWarmthSwing) + 1e-4f;
            CHECK(factor[0] <= limit);
            CHECK(factor[2] <= limit);
            CHECK(factor[0] > 0.0f);
            CHECK(factor[2] > 0.0f);
        }
    }
    CHECK(lowest >= 1.0f - Variation::kBrightnessSwing - 1e-4f);
    CHECK(highest <= 1.0f + Variation::kBrightnessSwing + 1e-4f);
    CHECK(highest - lowest > Variation::kBrightnessSwing); // it does vary across a map

    // Neighbouring vertices differ only a little: smooth patches, not speckle.
    float a[3];
    float b[3];
    for (int x = 0; x < 200; ++x)
    {
        Variation::MacroFactor(x, 50, 3, 1.0f, a);
        Variation::MacroFactor(x + 1, 50, 3, 1.0f, b);
        CHECK(std::fabs(a[1] - b[1]) < 0.05f);
    }
}

TEST_CASE("Terrain variation: block flips are stable and all four occur")
{
    int seen[4] = {};
    for (int by = 0; by < 16; ++by)
    {
        for (int bx = 0; bx < 16; ++bx)
        {
            const auto flip = Variation::BlockFlip(bx, by, 2, 0);
            const auto again = Variation::BlockFlip(bx, by, 2, 0);
            CHECK(flip.u == again.u);
            CHECK(flip.v == again.v);
            ++seen[(flip.u ? 1 : 0) + (flip.v ? 2 : 0)];
        }
    }
    for (int count : seen)
        CHECK(count > 20);
}

TEST_CASE("Terrain variation: mirroring keeps a tile inside its repeat")
{
    // The second tile of a 4-tile repeat starting at u 3, v 1: u 3.25..3.5, v 1.75..2.
    float uv[4][2] = {{3.25f, 1.75f}, {3.5f, 1.75f}, {3.5f, 2.0f}, {3.25f, 2.0f}};
    Variation::MirrorTileUv(uv, {true, true});
    CHECK(uv[0][0] == doctest::Approx(3.75f));
    CHECK(uv[1][0] == doctest::Approx(3.5f));
    CHECK(uv[0][1] == doctest::Approx(1.25f));
    CHECK(uv[2][1] == doctest::Approx(1.0f));

    float plain[4][2] = {{0.0f, 0.0f}, {0.25f, 0.0f}, {0.25f, 0.25f}, {0.0f, 0.25f}};
    Variation::MirrorTileUv(plain, {false, false});
    CHECK(plain[1][0] == doctest::Approx(0.25f));
    Variation::MirrorTileUv(plain, {true, false});
    CHECK(plain[0][0] == doctest::Approx(1.0f));
    CHECK(plain[1][0] == doctest::Approx(0.75f));
    CHECK(plain[0][1] == doctest::Approx(0.0f));
}
