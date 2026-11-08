#include <doctest.h>

#include "Render/Textures/OztFile.h"
#include "Render/Textures/PackedOzt.h"

#include <webp/encode.h>

#include <cstdint>
#include <vector>

namespace
{
namespace Packed = Render::Textures::PackedOzt;

void PutUInt16(std::vector<std::uint8_t>& out, std::size_t at, unsigned value)
{
    out[at] = static_cast<std::uint8_t>(value);
    out[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void AppendUInt(std::vector<std::uint8_t>& out, std::uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
    {
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

// An OZT of width x height with every pixel different, some fully transparent
// with colour under them, and a few bytes after the pixels.
std::vector<std::uint8_t> MakeOzt(int width, int height, const std::vector<std::uint8_t>& tail)
{
    std::vector<std::uint8_t> file(Render::Textures::Ozt::HeaderSize, 0);
    file[0] = 'M';
    file[1] = 'U';
    file[2] = 'X';
    file[3] = 2;
    file[4 + 2] = 2; // uncompressed true colour
    PutUInt16(file, 4 + 12, static_cast<unsigned>(width));
    PutUInt16(file, 4 + 14, static_cast<unsigned>(height));
    file[4 + 16] = 32;
    file[4 + 17] = 8;
    for (int i = 0; i < width * height; ++i)
    {
        file.push_back(static_cast<std::uint8_t>(i * 7));
        file.push_back(static_cast<std::uint8_t>(i * 13 + 1));
        file.push_back(static_cast<std::uint8_t>(i * 31 + 2));
        file.push_back(i % 5 == 0 ? 0 : static_cast<std::uint8_t>(i * 3));
    }
    file.insert(file.end(), tail.begin(), tail.end());
    return file;
}

// What scripts/hd_pack.py writes: the pixels as a lossless WebP with exact on.
std::vector<std::uint8_t> Pack(const std::vector<std::uint8_t>& ozt, int width, int height, std::size_t tailSize)
{
    const std::size_t head = Render::Textures::Ozt::HeaderSize;
    WebPConfig config;
    REQUIRE(WebPConfigInit(&config));
    config.lossless = 1;
    config.exact = 1;
    WebPPicture picture;
    REQUIRE(WebPPictureInit(&picture));
    picture.use_argb = 1;
    picture.width = width;
    picture.height = height;
    REQUIRE(WebPPictureImportBGRA(&picture, ozt.data() + head, width * 4));
    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);
    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = &writer;
    REQUIRE(WebPEncode(&config, &picture));

    std::vector<std::uint8_t> packed = {'M', 'U', 'W', '1'};
    AppendUInt(packed, static_cast<std::uint32_t>(ozt.size()), 4);
    AppendUInt(packed, static_cast<std::uint32_t>(head), 2);
    AppendUInt(packed, static_cast<std::uint32_t>(tailSize), 4);
    packed.insert(packed.end(), ozt.begin(), ozt.begin() + static_cast<std::ptrdiff_t>(head));
    packed.insert(packed.end(), ozt.end() - static_cast<std::ptrdiff_t>(tailSize), ozt.end());
    packed.insert(packed.end(), writer.mem, writer.mem + writer.size);
    WebPMemoryWriterClear(&writer);
    WebPPictureFree(&picture);
    return packed;
}
} // namespace

TEST_CASE("a packed OZT unpacks to the original file byte for byte")
{
    const std::vector<std::uint8_t> tail = {'T', 'R', 'U', 'E', 'V', 'I', 'S', 'I', 'O', 'N'};
    const auto ozt = MakeOzt(37, 19, tail);
    const auto packed = Pack(ozt, 37, 19, tail.size());

    CHECK(Packed::IsPacked(packed));
    CHECK_FALSE(Packed::IsPacked(ozt));
    std::vector<std::uint8_t> out;
    REQUIRE(Packed::Unpack(packed, out));
    CHECK(out == ozt);
}

TEST_CASE("a file without a tail unpacks too")
{
    const auto ozt = MakeOzt(8, 8, {});
    std::vector<std::uint8_t> out;
    REQUIRE(Packed::Unpack(Pack(ozt, 8, 8, 0), out));
    CHECK(out == ozt);
}

TEST_CASE("a broken packed file is refused")
{
    const auto ozt = MakeOzt(16, 4, {});
    auto packed = Pack(ozt, 16, 4, 0);
    std::vector<std::uint8_t> out;

    SUBCASE("cut short")
    {
        packed.resize(packed.size() / 2);
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("wrong original size")
    {
        packed[4] ^= 1;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("head shorter than the OZT header")
    {
        packed[8] = 3;
        packed[9] = 0;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("prefix only")
    {
        packed.resize(Packed::PrefixSize);
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    CHECK(out.empty());
}
