#include <cstdint>
#include <vector>

#include <doctest.h>

#include "Render/Textures/HdTextureSize.h"

namespace
{
namespace HdSize = Render::Textures::HdSize;

constexpr int IntegratedRamMb = 32 * 1024;
} // namespace

TEST_CASE("a saved size outside the list is Auto [render][hd_texture_size]")
{
    CHECK(HdSize::SettingFromConfig(0) == HdSize::Setting::Auto);
    CHECK(HdSize::SettingFromConfig(1) == HdSize::Setting::Full);
    CHECK(HdSize::SettingFromConfig(2) == HdSize::Setting::Half);
    CHECK(HdSize::SettingFromConfig(-1) == HdSize::Setting::Auto);
    CHECK(HdSize::SettingFromConfig(3) == HdSize::Setting::Auto);
}

TEST_CASE("the player's size wins over the machine [render][hd_texture_size]")
{
    CHECK_FALSE(HdSize::ChooseHalf(HdSize::Setting::Full, true, 4 * 1024));
    CHECK(HdSize::ChooseHalf(HdSize::Setting::Half, false, 64 * 1024));
}

TEST_CASE("Auto halves on shared-memory graphics and on little memory [render][hd_texture_size]")
{
    CHECK(HdSize::ChooseHalf(HdSize::Setting::Auto, true, IntegratedRamMb));
    CHECK(HdSize::ChooseHalf(HdSize::Setting::Auto, false, 8 * 1024));
    CHECK(HdSize::ChooseHalf(HdSize::Setting::Auto, false, HdSize::kAutoHalfBelowSystemRamMb - 1));
    CHECK_FALSE(HdSize::ChooseHalf(HdSize::Setting::Auto, false, HdSize::kAutoHalfBelowSystemRamMb));
    CHECK_FALSE(HdSize::ChooseHalf(HdSize::Setting::Auto, false, 16 * 1024));
    // Unknown memory alone never halves.
    CHECK_FALSE(HdSize::ChooseHalf(HdSize::Setting::Auto, false, 0));
}

TEST_CASE("only an HD copy with even sides is halved [render][hd_texture_size]")
{
    CHECK(HdSize::Divisor(true, 4, 1024, 512) == 2);
    CHECK(HdSize::Divisor(false, 4, 1024, 512) == 1);
    // An original file, and a copy that would stop counting as upscaled art.
    CHECK(HdSize::Divisor(true, 1, 256, 256) == 1);
    CHECK(HdSize::Divisor(true, 2, 512, 512) == 1);
    CHECK(HdSize::Divisor(true, 4, 1023, 512) == 1);
    CHECK(HdSize::Divisor(true, 4, 1024, 511) == 1);
    CHECK(HdSize::Divisor(true, 4, 0, 0) == 1);
}

TEST_CASE("an opaque image halves to the average of each block [render][hd_texture_size]")
{
    // 4 x 2: the left block averages to 25, 50, 75; the right one is uniform.
    const std::vector<std::uint8_t> rgba = {
        10, 20, 30, 255, 20, 40, 60,  255, 9, 9, 9, 255, 9, 9, 9, 255,
        30, 60, 90, 255, 40, 80, 120, 255, 9, 9, 9, 255, 9, 9, 9, 255,
    };
    const std::vector<std::uint8_t> half = HdSize::HalveRgba(rgba, 4, 2, true);
    CHECK(half == std::vector<std::uint8_t>{25, 50, 75, 255, 9, 9, 9, 255});
}

TEST_CASE("hidden texels keep their colour out of a cut-out edge [render][hd_texture_size]")
{
    // One visible red texel among three transparent green ones.
    const std::vector<std::uint8_t> rgba = {
        200, 0, 0, 255, 0, 255, 0, 0,
        0,   255, 0, 0, 0, 255, 0, 0,
    };
    const std::vector<std::uint8_t> straight = HdSize::HalveRgba(rgba, 2, 2, true);
    CHECK(straight == std::vector<std::uint8_t>{200, 0, 0, 64});
    // Premultiplied pixels are averaged as they are.
    const std::vector<std::uint8_t> plain = HdSize::HalveRgba(rgba, 2, 2, false);
    CHECK(plain == std::vector<std::uint8_t>{50, 191, 0, 64});
}

TEST_CASE("a fully transparent block keeps its plain colour [render][hd_texture_size]")
{
    const std::vector<std::uint8_t> rgba = {
        10, 20, 30, 0, 30, 40, 50, 0,
        10, 20, 30, 0, 30, 40, 50, 0,
    };
    CHECK(HdSize::HalveRgba(rgba, 2, 2, true) == std::vector<std::uint8_t>{20, 30, 40, 0});
}

TEST_CASE("an image that can't be halved gives nothing [render][hd_texture_size]")
{
    const std::vector<std::uint8_t> rgba(3 * 2 * 4, 255);
    CHECK(HdSize::HalveRgba(rgba, 3, 2, true).empty());
    CHECK(HdSize::HalveRgba(rgba, 2, 3, true).empty());
    // Fewer bytes than width x height texels.
    CHECK(HdSize::HalveRgba(rgba, 4, 2, true).empty());
    CHECK(HdSize::HalveRgba({}, 0, 0, true).empty());
}
