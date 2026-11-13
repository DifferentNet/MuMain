#include <doctest.h>

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_define.h"
#include "Render/Lighting/DynamicLight.h"
#include "Render/Renderer/RenderUtils.h"
#include "Render/Terrain/TerrainLightGrid.h"
#include "Render/Terrain/TorchLights.h"

#include <array>
#include <cstdint>
#include <vector>

namespace
{
namespace TorchLights = Render::Terrain::TorchLights;
using doctest::Approx;
using Render::Lighting::DynamicLight;
using TorchLights::Block;

DynamicLight MapLight(float x, float y, float brightness = 0.5f, int range = 3)
{
    const float color[3] = {brightness, brightness * 0.6f, brightness * 0.2f};
    DynamicLight light = Render::Terrain::LightGrid::MakeDynamicLight(x, y, color, range, 0.0f);
    light.fromMap = true;
    return light;
}

constexpr float kHero[3] = {12800.0f, 12800.0f, 0.0f};

using Grid = std::vector<std::array<float, 3>>;

Grid EmptyGrid()
{
    return Grid(static_cast<std::size_t>(TERRAIN_SIZE) * TERRAIN_SIZE, {0.0f, 0.0f, 0.0f});
}

float (*Data(Grid& grid))[3]
{
    return reinterpret_cast<float (*)[3]>(grid.data());
}

int Index(int x, int y)
{
    return y * TERRAIN_SIZE + x;
}

// The packed colour bytes (r, g, b) of a light.
std::array<int, 3> Bytes(const float light[3], const float* factor)
{
    const std::uint32_t packed = factor ? mu::PackABGR(light[0] * factor[0], light[1] * factor[1],
                                                       light[2] * factor[2], 1.0f)
                                        : mu::PackABGR(light[0], light[1], light[2], 1.0f);
    return {static_cast<int>(packed & 0xFFu), static_cast<int>((packed >> 8) & 0xFFu),
            static_cast<int>((packed >> 16) & 0xFFu)};
}
} // namespace

TEST_CASE("the strength is TorchLightsStrength over 100, clamped, and off when not available [render][torch_lights]")
{
    CHECK(TorchLights::Resolve(1, 70, true, false) == Approx(0.7f));
    CHECK(TorchLights::Resolve(1, 100, true, false) == Approx(1.0f));
    CHECK(TorchLights::Resolve(1, 250, true, false) == Approx(1.0f));
    CHECK(TorchLights::Resolve(1, -5, true, false) == 0.0f);
    CHECK(TorchLights::Resolve(1, 0, true, false) == 0.0f);
    CHECK(TorchLights::Resolve(1, 70, false, false) == 0.0f);
    CHECK(TorchLights::Resolve(1, 70, true, true) == 0.0f);
    CHECK(TorchLights::Resolve(0, 70, true, false) == 0.0f);
    CHECK(TorchLights::Resolve(2, 70, true, false) == 0.0f);
}

TEST_CASE("the gain is 0.5 plus the strength, 1.2 at the default [render][torch_lights]")
{
    CHECK(TorchLights::Gain(0.0f) == Approx(0.5f));
    CHECK(TorchLights::Gain(TorchLights::kDefaultPercent / 100.0f) == Approx(1.2f));
    CHECK(TorchLights::Gain(1.0f) == Approx(1.5f));
    CHECK(TorchLights::Gain(3.0f) == Approx(1.5f));
}

TEST_CASE("the row is locked by the first reason that applies [render][torch_lights]")
{
    CHECK(TorchLights::ResolveBlock(true, false, true) == Block::None);
    CHECK(TorchLights::ResolveBlock(false, true, false) == Block::NoHdTextures);
    CHECK(TorchLights::ResolveBlock(true, true, false) == Block::LowPreset);
    CHECK(TorchLights::ResolveBlock(true, false, false) == Block::NoDynamicLights);
    CHECK_FALSE(TorchLights::Locks(Block::None));
    CHECK(TorchLights::Locks(Block::NoHdTextures));
    CHECK(TorchLights::Locks(Block::LowPreset));
    CHECK(TorchLights::Locks(Block::NoDynamicLights));
}

TEST_CASE("only map lights with a range and no darkening are selected [render][torch_lights]")
{
    std::vector<DynamicLight> lights;
    lights.push_back(MapLight(kHero[0] + 100.0f, kHero[1]));
    DynamicLight effect = MapLight(kHero[0] + 50.0f, kHero[1]);
    effect.fromMap = false;
    lights.push_back(effect);
    // Far enough that its grid square does not touch the selected light's.
    DynamicLight darkening = MapLight(kHero[0] - 2000.0f, kHero[1]);
    darkening.color[2] = -0.1f;
    lights.push_back(darkening);
    DynamicLight rangeless = MapLight(kHero[0] + 70.0f, kHero[1]);
    rangeless.range = 0;
    lights.push_back(rangeless);
    lights.push_back(MapLight(kHero[0] + TorchLights::kMaxDistance + 1.0f, kHero[1]));

    TorchLights::Selection selection{};
    REQUIRE(TorchLights::Select(lights, kHero, selection) == 1);
    CHECK(selection[0].x == Approx(kHero[0] + 100.0f));
    CHECK(selection[0].weight == Approx(1.0f));
    CHECK(selection[0].range == 3);
    CHECK(selection[0].radius == Approx(300.0f * TorchLights::kRadiusScale));
    CHECK(selection[0].color[1] == Approx(0.3f));
}

TEST_CASE("a map light whose grid square touches a darkening light's is left out [render][torch_lights]")
{
    std::vector<DynamicLight> lights;
    // Range 3 each: squares of 7 cells; 600 units apart they share a column of vertices, 700 apart they do not.
    lights.push_back(MapLight(kHero[0] + 100.0f, kHero[1]));
    lights.push_back(MapLight(kHero[0] + 2000.0f, kHero[1]));
    DynamicLight darkening = MapLight(kHero[0] + 700.0f, kHero[1]);
    darkening.fromMap = false;
    darkening.color[0] = -0.6f;
    lights.push_back(darkening);

    TorchLights::Selection selection{};
    REQUIRE(TorchLights::Select(lights, kHero, selection) == 1);
    CHECK(selection[0].x == Approx(kHero[0] + 2000.0f));

    lights[2].position[0] = kHero[0] + 800.0f;
    REQUIRE(TorchLights::Select(lights, kHero, selection) == 2);
    CHECK(selection[0].x == Approx(kHero[0] + 100.0f));

    // The grid wraps at the map edge, and so does the overlap test.
    std::vector<DynamicLight> edge;
    edge.push_back(MapLight(50.0f, kHero[1]));
    DynamicLight across = MapLight(TERRAIN_SIZE * TERRAIN_SCALE - 50.0f, kHero[1]);
    across.fromMap = false;
    across.color[1] = -0.2f;
    edge.push_back(across);
    const float nearEdge[3] = {50.0f, kHero[1], 0.0f};
    CHECK(TorchLights::Select(edge, nearEdge, selection) == 0);
}

TEST_CASE("the 16 nearest are kept, nearest first, fading out before the 17th [render][torch_lights]")
{
    std::vector<DynamicLight> lights;
    // 20 lights 100 units apart on a line, farthest first.
    for (int i = 20; i >= 1; --i)
        lights.push_back(MapLight(kHero[0] + 100.0f * static_cast<float>(i), kHero[1]));

    TorchLights::Selection selection{};
    const std::size_t count = TorchLights::Select(lights, kHero, selection);
    // The 17th is at 1700: the 16th (1600) fades by 100 / 400, the 13th (1300) is the last at full weight.
    REQUIRE(count == 16);
    for (std::size_t i = 0; i < count; ++i)
        CHECK(selection[i].x == Approx(kHero[0] + 100.0f * static_cast<float>(i + 1)));
    CHECK(selection[12].weight == Approx(1.0f));
    CHECK(selection[13].weight == Approx(0.75f));
    CHECK(selection[14].weight == Approx(0.5f));
    CHECK(selection[15].weight == Approx(0.25f));
}

TEST_CASE("lights near the distance limit fade out and drop at weight 0 [render][torch_lights]")
{
    std::vector<DynamicLight> lights;
    lights.push_back(MapLight(kHero[0] + TorchLights::kMaxDistance - 200.0f, kHero[1]));
    lights.push_back(MapLight(kHero[0], kHero[1] + TorchLights::kMaxDistance - 0.5f));
    TorchLights::Selection selection{};
    const std::size_t count = TorchLights::Select(lights, kHero, selection);
    REQUIRE(count == 2);
    CHECK(selection[0].weight == Approx(0.5f));
    CHECK(selection[1].weight == Approx(0.5f / TorchLights::kFadeBand));

    std::vector<DynamicLight> crowded;
    // 17 lights at the same distance: the cutoff is that distance, so none is drawn per pixel.
    for (int i = 0; i < 17; ++i)
        crowded.push_back(MapLight(kHero[0] + 500.0f, kHero[1]));
    CHECK(TorchLights::Select(crowded, kHero, selection) == 0);
}

TEST_CASE("the shader block holds the count, positions, radii and colour * weight * gain [render][torch_lights]")
{
    TorchLights::Selected a;
    a.x = 100.0f;
    a.y = 200.0f;
    a.radius = 345.0f;
    a.weight = 0.5f;
    a.color[0] = 0.8f;
    a.color[1] = 0.4f;
    a.color[2] = 0.2f;
    a.range = 3;
    TorchLights::Selected b = a;
    b.x = 300.0f;
    b.weight = 1.0f;
    const std::array<TorchLights::Selected, 2> selected{a, b};
    const mu::TerrainLightsUniform uniform = TorchLights::Pack(selected, 1.2f);
    CHECK(uniform.header[0] == 2.0f);
    CHECK(uniform.positionAndRadius[0][0] == 100.0f);
    CHECK(uniform.positionAndRadius[0][1] == 200.0f);
    CHECK(uniform.positionAndRadius[0][3] == 345.0f);
    CHECK(uniform.color[0][0] == Approx(0.8f * 0.5f * 1.2f));
    CHECK(uniform.color[0][2] == Approx(0.2f * 0.5f * 1.2f));
    CHECK(uniform.positionAndRadius[1][0] == 300.0f);
    CHECK(uniform.color[1][1] == Approx(0.4f * 1.2f));
    CHECK(uniform.positionAndRadius[2][3] == 0.0f);
    CHECK(uniform.color[2][0] == 0.0f);
}

TEST_CASE("a weight 1 share is exactly what the light added to the grid [render][torch_lights]")
{
    const DynamicLight light = MapLight(5050.0f, 7020.0f, 0.7f, 3);
    Grid grid = EmptyGrid();
    Render::Terrain::LightGrid::AddLight(Data(grid), light.position[0], light.position[1], light.color, light.range);

    TorchLights::Selection selection{};
    const std::array<DynamicLight, 1> lights{light};
    REQUIRE(TorchLights::Select(lights, light.position, selection) == 1);
    TorchLights::ShareGrid shares;
    shares.Rebuild({selection.data(), 1});
    for (int y = 60; y < 80; ++y)
    {
        for (int x = 40; x < 60; ++x)
        {
            const float* share = shares.At(Index(x, y));
            for (int channel = 0; channel < 3; ++channel)
                CHECK(share[channel] == grid[static_cast<std::size_t>(Index(x, y))][channel]);
        }
    }
}

TEST_CASE("a rebuild leaves nothing of the last frame's lights [render][torch_lights]")
{
    TorchLights::ShareGrid shares;
    const float* before = shares.At(Index(10, 10));
    CHECK(before[0] == 0.0f);

    TorchLights::Selection selection{};
    const std::array<DynamicLight, 2> first{MapLight(1000.0f, 1000.0f), MapLight(25550.0f, 50.0f)};
    const float reference[3] = {1000.0f, 1000.0f, 0.0f};
    const std::size_t count = TorchLights::Select(first, reference, selection);
    REQUIRE(count == 1);
    shares.Rebuild({selection.data(), count});
    CHECK(shares.At(Index(10, 10))[0] > 0.0f);

    const std::array<DynamicLight, 1> second{MapLight(3000.0f, 3000.0f)};
    REQUIRE(TorchLights::Select(second, reference, selection) == 1);
    shares.Rebuild({selection.data(), 1});
    for (int y = 0; y < TERRAIN_SIZE; ++y)
    {
        for (int x = 0; x < TERRAIN_SIZE; ++x)
        {
            const bool nearSecond = x >= 26 && x <= 34 && y >= 26 && y <= 34;
            if (!nearSecond)
            {
                const float* share = shares.At(Index(x, y));
                REQUIRE(share[0] == 0.0f);
                REQUIRE(share[1] == 0.0f);
                REQUIRE(share[2] == 0.0f);
            }
        }
    }

    // A light whose square wraps over the grid's edge is cleared where it wrapped too.
    const std::array<DynamicLight, 1> edge{MapLight(50.0f, 50.0f)};
    REQUIRE(TorchLights::Select(edge, reference, selection) == 1);
    shares.Rebuild({selection.data(), 1});
    CHECK(shares.At(Index(TERRAIN_SIZE - 1, TERRAIN_SIZE - 1))[0] > 0.0f);
    shares.Clear();
    for (int y = 0; y < TERRAIN_SIZE; ++y)
        for (int x = 0; x < TERRAIN_SIZE; ++x)
            REQUIRE(shares.At(Index(x, y))[0] == 0.0f);
}

TEST_CASE("the removable part gives the bytes of the light without the share, exactly [render][torch_lights]")
{
    const float factor[3] = {1.07f, 0.93f, 1.01f};
    const float lights[][3] = {{0.3f, 0.5f, 0.7f}, {0.9f, 0.8f, 0.6f}, {1.4f, 1.1f, 0.2f}, {0.0f, 0.05f, 0.95f}};
    const float shares[][3] = {{0.2f, 0.1f, 0.05f}, {0.5f, 0.5f, 0.5f}, {0.6f, 0.3f, 0.4f}, {0.01f, 0.2f, 0.0f}};
    for (const auto& light : lights)
    {
        for (const auto& share : shares)
        {
            for (const float* f : {static_cast<const float*>(nullptr), static_cast<const float*>(factor)})
            {
                float out[3];
                TorchLights::RemovablePart(light, share, f, out);
                const float base[3] = {(std::max)(light[0] - share[0], 0.0f), (std::max)(light[1] - share[1], 0.0f),
                                       (std::max)(light[2] - share[2], 0.0f)};
                const std::array<int, 3> full = Bytes(light, f);
                const std::array<int, 3> expected = Bytes(base, f);
                for (int channel = 0; channel < 3; ++channel)
                {
                    CHECK(out[channel] >= 0.0f);
                    CHECK(out[channel] <= 1.0f);
                    // What the shader takes away, back in bytes, leaves exactly the base light's byte.
                    const int removed = static_cast<int>(out[channel] * 255.0f + 0.5f);
                    CHECK(full[channel] - removed == expected[channel]);
                }
            }
        }
    }
}

TEST_CASE("without a share the removable part is zero [render][torch_lights]")
{
    const float light[3] = {0.4f, 0.4f, 0.4f};
    const float none[3] = {0.0f, 0.0f, 0.0f};
    float out[3] = {1.0f, 1.0f, 1.0f};
    TorchLights::RemovablePart(light, none, nullptr, out);
    CHECK(out[0] == 0.0f);
    CHECK(out[1] == 0.0f);
    CHECK(out[2] == 0.0f);
}
