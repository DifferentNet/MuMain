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

TEST_CASE("the layout check adds the sizes in 64 bits")
{
    constexpr std::uint64_t head = 0x200;
    constexpr std::uint64_t tail = 0xFFFFFF00;
    // In 32 bits head + tail wraps round to 0x100, which would fit this file and leave all of
    // `size` for the pixels.
    static_assert(static_cast<std::uint32_t>(head + tail) == 0x100u);
    constexpr std::uint64_t fileSize = Packed::PrefixSize + 0x1000;
    constexpr std::uint64_t size = 0x1100;
    CHECK_FALSE(Packed::CheckLayout(fileSize, size, head, tail, size - 0x100));
    CHECK_FALSE(Packed::CheckLayout(fileSize, size, head, tail, size - static_cast<std::uint32_t>(head + tail)));
    // The same tail against an original of 4 GB and more: still more than the file holds.
    CHECK_FALSE(Packed::CheckLayout(fileSize, 0x1'0000'0100ull, head, tail, 0x100));

    // A layout that adds up, and each way it can be off by one.
    constexpr std::uint64_t okHead = Render::Textures::Ozt::HeaderSize;
    constexpr std::uint64_t okTail = 3;
    constexpr std::uint64_t pixels = 16;
    constexpr std::uint64_t okSize = okHead + pixels + okTail;
    constexpr std::uint64_t okFile = Packed::PrefixSize + okHead + okTail + 40;
    CHECK(Packed::CheckLayout(okFile, okSize, okHead, okTail, pixels));
    CHECK_FALSE(Packed::CheckLayout(okFile, okSize, okHead - 1, okTail, pixels + 1));
    CHECK_FALSE(Packed::CheckLayout(okFile, okSize, okHead, okTail, pixels + 1));
    CHECK_FALSE(Packed::CheckLayout(okFile, okSize + 1, okHead, okTail, pixels));
    CHECK(Packed::CheckLayout(Packed::PrefixSize + okHead + okTail, okSize, okHead, okTail, pixels));
    CHECK_FALSE(Packed::CheckLayout(Packed::PrefixSize + okHead + okTail - 1, okSize, okHead, okTail, pixels));
    CHECK_FALSE(Packed::CheckLayout(Packed::PrefixSize - 1, okSize, okHead, okTail, pixels));
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
    SUBCASE("tail size near 4 GB: head + tail would wrap round in 32 bits")
    {
        packed[10] = 0xF0;
        packed[11] = 0xFF;
        packed[12] = 0xFF;
        packed[13] = 0xFF;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("tail longer than the file")
    {
        const auto tail = static_cast<std::uint32_t>(packed.size());
        for (int i = 0; i < 4; ++i)
        {
            packed[10 + i] = static_cast<std::uint8_t>(tail >> (8 * i));
        }
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("head longer than the file")
    {
        packed[8] = 0xFF;
        packed[9] = 0xFF;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("original size smaller than head + tail")
    {
        packed[4] = 1;
        packed[5] = 0;
        packed[6] = 0;
        packed[7] = 0;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("OZT header width not the picture's")
    {
        packed[Packed::PrefixSize + 4 + 12] ^= 1;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("OZT header height not the picture's")
    {
        packed[Packed::PrefixSize + 4 + 14] ^= 1;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("OZT header not 32 bits a pixel")
    {
        packed[Packed::PrefixSize + 4 + 16] = 24;
        CHECK_FALSE(Packed::Unpack(packed, out));
    }
    SUBCASE("WebP data broken")
    {
        packed.back() ^= 0xFF;
        packed[packed.size() - 2] ^= 0xFF;
        packed[packed.size() / 2 + 20] ^= 0x55;
        // Either refused or, if libwebp still decodes it, nothing beyond the original size.
        if (Packed::Unpack(packed, out))
        {
            CHECK(out.size() == ozt.size());
            out.clear();
        }
    }
    CHECK(out.empty());
}

TEST_CASE("a picture wider than 4 times the largest original is refused before any allocation")
{
    // 4097 x 1: within libwebp's limits, beyond the HD art's (1024 x scale 4).
    const auto ozt = MakeOzt(4097, 1, {});
    const auto packed = Pack(ozt, 4097, 1, 0);
    std::vector<std::uint8_t> out;
    CHECK_FALSE(Packed::Unpack(packed, out));
    CHECK(out.empty());

    const auto widest = MakeOzt(4096, 1, {});
    REQUIRE(Packed::Unpack(Pack(widest, 4096, 1, 0), out));
    CHECK(out == widest);
}
