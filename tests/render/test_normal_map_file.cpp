#include <doctest.h>

#include "Render/Textures/NormalMapFile.h"
#include "Render/Textures/NormalMaps.h"

#include <webp/encode.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
namespace File = Render::Textures::NormalMapFile;
namespace NormalMaps = Render::Textures::NormalMaps;

// One grey picture of a map file (lossless here, so the test knows every value).
std::vector<std::uint8_t> GreyWebp(int width, int height, int base, int step, int period)
{
    std::vector<std::uint8_t> rgb;
    for (int i = 0; i < width * height; ++i)
    {
        const auto value = static_cast<std::uint8_t>(base + step * (i % period));
        rgb.insert(rgb.end(), {value, value, value});
    }
    std::uint8_t* webp = nullptr;
    const std::size_t size = WebPEncodeLosslessRGB(rgb.data(), width, height, width * 3, &webp);
    REQUIRE(size > 0);
    std::vector<std::uint8_t> out(webp, webp + size);
    WebPFree(webp);
    return out;
}

// A map file laid out as scripts/hd_pack.py writes it: x tilt 100 + i % 50, y tilt 140 - i % 30.
std::vector<std::uint8_t> MakeFile(int width, int height)
{
    const auto red = GreyWebp(width, height, 100, 1, 50);
    const auto green = GreyWebp(width, height, 140, -1, 30);
    const auto redSize = static_cast<std::uint32_t>(red.size());
    std::vector<std::uint8_t> file = {'M',
                                      'U',
                                      'N',
                                      '2',
                                      static_cast<std::uint8_t>(width),
                                      static_cast<std::uint8_t>(width >> 8),
                                      static_cast<std::uint8_t>(height),
                                      static_cast<std::uint8_t>(height >> 8),
                                      static_cast<std::uint8_t>(redSize),
                                      static_cast<std::uint8_t>(redSize >> 8),
                                      static_cast<std::uint8_t>(redSize >> 16),
                                      static_cast<std::uint8_t>(redSize >> 24)};
    file.insert(file.end(), red.begin(), red.end());
    file.insert(file.end(), green.begin(), green.end());
    return file;
}

struct Disk
{
    std::wstring path;
    std::vector<std::uint8_t> bytes;
    int reads = 0;
    std::size_t askedMax = 0;

    File::Reader Reader()
    {
        return [this](const std::wstring& asked, std::size_t maxBytes, std::vector<std::uint8_t>& out) {
            ++reads;
            askedMax = maxBytes;
            if (asked != path)
            {
                return false;
            }
            out = bytes;
            return true;
        };
    }
};

struct MapsOn
{
    explicit MapsOn(bool on) { NormalMaps::SetEnabled(on); }
    ~MapsOn() { NormalMaps::SetEnabled(false); }
};
} // namespace

TEST_CASE("a prebuilt map sits next to its texture at half its size [render][textures][normalmaps]")
{
    CHECK(File::MaxFileSize(8, 4) == File::PrefixSize + 4u * 2u * 4u + 64u * 1024u);
    CHECK(File::PathFor(L"Data\\HD\\Item\\Sword01.OZJ") == L"Data\\HD\\Item\\Sword01.OZJ.nrm");
    CHECK(File::HalfSize(512) == 256);
    CHECK(File::HalfSize(5) == 3);
    CHECK(File::HalfSize(1) == 1);
}

TEST_CASE("a prebuilt map decodes to its tilt with blue and alpha 255 [render][textures][normalmaps]")
{
    const auto file = MakeFile(5, 3);
    const auto pixels = File::Decode(file, 10, 6);
    REQUIRE(pixels.size() == 5u * 3u * 4u);
    CHECK(pixels[0] == 100);
    CHECK(pixels[1] == 140);
    CHECK(pixels[2] == 255);
    CHECK(pixels[3] == 255);
    CHECK(pixels[4 * 7] == 107);
    CHECK(pixels[4 * 7 + 1] == 133);
    // A texture of odd size: its map rounds up.
    CHECK(File::Decode(file, 9, 5).size() == pixels.size());
}

TEST_CASE("a broken or mismatched map file is refused [render][textures][normalmaps]")
{
    auto file = MakeFile(4, 4);
    CHECK_FALSE(File::Decode(file, 8, 8).empty());

    SUBCASE("for a texture of another size")
    {
        CHECK(File::Decode(file, 16, 8).empty());
        CHECK(File::Decode(file, 8, 6).empty());
        CHECK(File::Decode(file, 0, 8).empty());
    }
    SUBCASE("header size not the picture's")
    {
        file[4] = 2;
        CHECK(File::Decode(file, 4, 8).empty());
    }
    SUBCASE("wrong mark")
    {
        file[3] = '3';
        CHECK(File::Decode(file, 8, 8).empty());
    }
    SUBCASE("an old MUN1 file")
    {
        file[3] = '1';
        CHECK(File::Decode(file, 8, 8).empty());
    }
    SUBCASE("first picture's size past the end")
    {
        file[8] = 0xFF;
        file[9] = 0xFF;
        CHECK(File::Decode(file, 8, 8).empty());
    }
    SUBCASE("no second picture")
    {
        const std::size_t firstSize = file[8] | (file[9] << 8);
        file.resize(File::PrefixSize + firstSize + 1);
        CHECK(File::Decode(file, 8, 8).empty());
    }
    SUBCASE("prefix only")
    {
        file.resize(File::PrefixSize);
        CHECK(File::Decode(file, 8, 8).empty());
    }
    SUBCASE("empty")
    {
        CHECK(File::Decode({}, 8, 8).empty());
    }
    SUBCASE("WebP cut short")
    {
        file.resize(file.size() - 6);
        CHECK(File::Decode(file, 8, 8).empty());
    }
}

TEST_CASE("the Remaster loads the map of a model texture from disk [render][textures][normalmaps]")
{
    const MapsOn on(true);
    Disk disk{L"Data\\HD\\Item\\Sword01.OZJ.nrm", MakeFile(4, 2)};
    CHECK(File::Load(L"Data\\HD\\Item\\Sword01.OZJ", 8, 4, disk.Reader()).size() == 4u * 2u * 4u);
    CHECK(disk.reads == 1);
    CHECK(disk.askedMax == File::MaxFileSize(8, 4));

    SUBCASE("a missing file: no relief, nothing built")
    {
        CHECK(File::Load(L"Data\\HD\\Item\\Sword02.OZJ", 8, 4, disk.Reader()).empty());
    }
    SUBCASE("a broken file: no relief")
    {
        disk.bytes.resize(disk.bytes.size() / 2);
        CHECK(File::Load(L"Data\\HD\\Item\\Sword01.OZJ", 8, 4, disk.Reader()).empty());
    }
    SUBCASE("a file larger than a map can be: refused")
    {
        disk.bytes.resize(File::MaxFileSize(8, 4) + 1u, 0);
        CHECK(File::Load(L"Data\\HD\\Item\\Sword01.OZJ", 8, 4, disk.Reader()).empty());
    }
    SUBCASE("textures that get no map are not looked up")
    {
        CHECK(File::Load(L"Data\\HD\\World1\\TileGrass01.OZJ", 8, 4, disk.Reader()).empty());
        CHECK(File::Load(L"Data\\Item\\Sword01.OZJ", 8, 4, disk.Reader()).empty());
        CHECK(disk.reads == 1);
    }
}

TEST_CASE("the Classic client never reads a map [render][textures][normalmaps]")
{
    const MapsOn off(false);
    Disk disk{L"Data\\HD\\Item\\Sword01.OZJ.nrm", MakeFile(4, 2)};
    CHECK(File::Load(L"Data\\HD\\Item\\Sword01.OZJ", 8, 4, disk.Reader()).empty());
    CHECK(disk.reads == 0);
}
