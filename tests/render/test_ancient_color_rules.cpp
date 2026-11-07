#include <doctest.h>

#include "Render/Items/AncientColorRules.h"

#include <array>
#include <cstdint>

using namespace Render::Items::AncientColors;

namespace
{
// Itemsettype.bmd numbers of a few sets (checked against the server's database, 2026-11-07).
constexpr int Kantata = 9;
constexpr int Rave = 10;
constexpr int Karis = 28;
constexpr int Apis = 56;
constexpr int Mist = 4;

std::array<std::uint8_t, 4> RecolorOne(std::array<std::uint8_t, 4> pixel, int set)
{
    Recolor(pixel.data(), 1, 4, *FindSetColor(set));
    return pixel;
}
} // namespace

TEST_CASE("every ancient set has its own colour entry")
{
    REQUIRE(FindSetColor(Kantata) != nullptr);
    REQUIRE(FindSetColor(Rave) != nullptr);
    CHECK(FindSetColor(0) == nullptr);
    CHECK(FindSetColor(200) == nullptr);

    int count = 0;
    for (int set = 1; set < 256; ++set)
    {
        if (FindSetColor(set) != nullptr)
        {
            ++count;
        }
    }
    CHECK(count == 46);
}

TEST_CASE("the two sets of a model differ")
{
    const SetColor& kantata = *FindSetColor(Kantata);
    const SetColor& rave = *FindSetColor(Rave);
    CHECK((kantata.r != rave.r || kantata.g != rave.g || kantata.b != rave.b));
}

TEST_CASE("the set follows the discriminator, or the other slot when its own is empty")
{
    CHECK(PickSet({Kantata, Rave}, 1) == Kantata);
    CHECK(PickSet({Kantata, Rave}, 2) == Rave);
    // Spirit helm: the client file has Karis in the first slot, the server says discriminator 2.
    CHECK(PickSet({Karis, 0}, 2) == Karis);
    CHECK(PickSet({0xFF, Karis}, 1) == Karis);
    CHECK(PickSet({0, 0}, 1) == 0);
    CHECK(PickSet({Kantata, Rave}, 0) == 0);
    CHECK(PickSet({Kantata, Rave}, 3) == 0);
}

TEST_CASE("skin, hair and faces keep their look")
{
    CHECK(KeepsTexture(L"Data\\Player\\hair_R.jpg"));
    CHECK(KeepsTexture(L"skin_barbarian_01.jpg"));
    CHECK(KeepsTexture(L"Data/Player/hide.jpg"));
    CHECK(KeepsTexture(L"level_man033.jpg"));
    CHECK(KeepsTexture(L"hair_R.jpg"));
    CHECK(KeepsTexture(L"richface.jpg"));
    CHECK(KeepsTexture(L"Skinclass206head_N.jpg"));
    CHECK_FALSE(KeepsTexture(L"upper_04_m.jpg"));
    CHECK_FALSE(KeepsTexture(L"Skinclass206_upper.jpg"));
    CHECK_FALSE(KeepsTexture(L"Data/Player/level/upper.jpg"));
}

TEST_CASE("colorize turns grey metal into the set's colour and keeps alpha")
{
    const auto out = RecolorOne({128, 128, 128, 77}, Kantata);
    CHECK(out[1] > out[0]);  // emerald: green above red
    CHECK(out[3] == 77);
}

TEST_CASE("hue keeps neutral trims and moves coloured parts")
{
    const auto grey = RecolorOne({150, 150, 150, 255}, Apis);
    CHECK(grey == std::array<std::uint8_t, 4>{150, 150, 150, 255});

    const auto red = RecolorOne({200, 40, 40, 255}, Apis);  // red cloth to azure
    CHECK(red[2] > red[0]);
}

TEST_CASE("skin tones stay")
{
    const std::array<std::uint8_t, 4> skin{230, 180, 140, 255};
    CHECK(RecolorOne(skin, Kantata) == skin);
    CHECK(RecolorOne(skin, Mist) == skin);
}

TEST_CASE("RGB pixels are recoloured three bytes at a time")
{
    std::array<std::uint8_t, 6> pixels{128, 128, 128, 128, 128, 128};
    Recolor(pixels.data(), 2, 3, *FindSetColor(Rave));
    CHECK(pixels[0] == pixels[3]);
    CHECK(pixels[0] > pixels[2]);  // bronze: red above blue
}

TEST_CASE("accent moves only the model's main colour; gold trims and silver stay")
{
    constexpr int Tyr = 57;  // Thunder Hawk: purple body, gold trims
    REQUIRE(FindSetColor(Tyr) != nullptr);
    CHECK(FindSetColor(Tyr)->tint == Tint::Accent);

    const auto purple = RecolorOne({90, 40, 160, 255}, Tyr);  // body to storm blue
    CHECK(purple[2] > purple[0]);
    CHECK(purple[1] > 40);

    const std::array<std::uint8_t, 4> gold{220, 170, 40, 255};
    CHECK(RecolorOne(gold, Tyr) == gold);
    const std::array<std::uint8_t, 4> silver{180, 180, 185, 255};
    CHECK(RecolorOne(silver, Tyr) == silver);
}
