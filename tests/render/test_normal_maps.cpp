#include "doctest.h"

#include "Render/Textures/NormalMaps.h"

#include <cstdint>
#include <vector>

using Render::Textures::NormalMaps::Build;
using Render::Textures::NormalMaps::WantsNormalMap;

TEST_CASE("model textures under Data\\HD get a normal map [render][textures][normalmaps]")
{
    CHECK(WantsNormalMap(L"Data\\HD\\Item\\Sword01.OZJ"));
    CHECK(WantsNormalMap(L"Data\\HD\\Player\\Item2001_Armor.OZJ"));
    CHECK(WantsNormalMap(L"data/hd/monster/Mon01.OZT"));
    CHECK(WantsNormalMap(L"Data\\HD\\NPC\\Npc01.OZJ"));
    CHECK(WantsNormalMap(L"Data\\HD\\Object74\\House.OZJ"));
}

TEST_CASE("ground, effects, the interface and originals get none [render][textures][normalmaps]")
{
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\World1\\TileGrass01.OZJ"));
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\Effect\\Flare.OZJ"));
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\Skill\\Fire.OZJ"));
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\Interface\\Frame.OZT"));
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\Object\\a.OZJ"));
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\Object7x\\a.OZJ"));
    CHECK_FALSE(WantsNormalMap(L"Data\\Item\\Sword01.OZJ"));
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\Item\\"));
    CHECK_FALSE(WantsNormalMap(L"Data\\HD\\Item"));
    CHECK_FALSE(WantsNormalMap(L""));
}

TEST_CASE("an even image is flat and a brighter right half leans the normal left [render][textures][normalmaps]")
{
    constexpr int size = 16;
    std::vector<std::uint8_t> even(size * size * 4, 100);
    for (std::size_t i = 3; i < even.size(); i += 4)
    {
        even[i] = 255;
    }
    const std::vector<std::uint8_t> flat = Build(even, size, size, 2);
    REQUIRE(flat.size() == even.size());
    for (std::size_t i = 0; i < flat.size(); i += 4)
    {
        CHECK(flat[i] == 128);
        CHECK(flat[i + 1] == 128);
    }

    std::vector<std::uint8_t> step = even;
    for (int y = 0; y < size; ++y)
    {
        for (int x = size / 2; x < size; ++x)
        {
            std::uint8_t* p = step.data() + (static_cast<std::size_t>(y) * size + x) * 4u;
            p[0] = p[1] = p[2] = 200;
        }
    }
    const std::vector<std::uint8_t> edge = Build(step, size, size, 2);
    const std::uint8_t* middle = edge.data() + (static_cast<std::size_t>(size / 2) * size + size / 2) * 4u;
    CHECK(middle[0] < 128);
    CHECK(middle[1] == 128);
}

TEST_CASE("a short buffer builds nothing [render][textures][normalmaps]")
{
    std::vector<std::uint8_t> pixels(10, 0);
    CHECK(Build(pixels, 4, 4, 1).empty());
    CHECK(Build(pixels, 0, 0, 1).empty());
}
