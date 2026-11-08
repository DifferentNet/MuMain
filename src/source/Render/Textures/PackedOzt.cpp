#include "PackedOzt.h"

#include "OztFile.h"
#include "Render/Sprites/BitmapTextureLimits.h"

#include <cstring>
#include <new>
#include <stdexcept>
#include <type_traits>

#if defined(MU_HAS_WEBP)
#include <webp/decode.h>
#endif

namespace Render::Textures::PackedOzt
{
namespace
{
constexpr std::uint8_t Magic[4] = {'M', 'U', 'W', '1'};

// The HD art is at most 4 times the largest original (UpscaleMarker: scale 2 or 4).
constexpr int MaximumScale = 4;
constexpr int MaximumWidth = Render::Sprites::kBitmapMaximumWidth * MaximumScale;
constexpr int MaximumHeight = Render::Sprites::kBitmapMaximumHeight * MaximumScale;

// Where the OZT header (after its 4-byte "MUX" marker) keeps width, height and bits per pixel.
constexpr std::size_t WidthOffset = 4 + 12;
constexpr std::size_t HeightOffset = 4 + 14;
constexpr std::size_t BitsOffset = 4 + 16;

std::uint32_t ReadUInt32(std::span<const std::uint8_t> file, std::size_t offset)
{
    return static_cast<std::uint32_t>(file[offset]) | (static_cast<std::uint32_t>(file[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(file[offset + 2]) << 16) | (static_cast<std::uint32_t>(file[offset + 3]) << 24);
}

std::uint16_t ReadUInt16(std::span<const std::uint8_t> file, std::size_t offset)
{
    return static_cast<std::uint16_t>(file[offset] | (file[offset + 1] << 8));
}
} // namespace

bool CheckLayout(std::uint64_t fileSize, std::uint64_t size, std::uint64_t headSize, std::uint64_t tailSize,
                 std::uint64_t pixelBytes)
{
    if (fileSize < PrefixSize || headSize < Ozt::HeaderSize)
    {
        return false;
    }
    // Two sizes of at most 32 bits each: their sum fits in 64 bits.
    const std::uint64_t sections = headSize + tailSize;
    return sections <= fileSize - PrefixSize && sections <= size && pixelBytes == size - sections;
}

bool IsPacked(std::span<const std::uint8_t> file)
{
    return file.size() >= PrefixSize && std::memcmp(file.data(), Magic, sizeof(Magic)) == 0;
}

bool Unpack(std::span<const std::uint8_t> file, std::vector<std::uint8_t>& out)
{
    out.clear();
    if (!IsPacked(file))
    {
        return false;
    }
#if defined(MU_HAS_WEBP)
    // Every size is checked in 64 bits (CheckLayout): on x86 a sum of two sizes from a broken
    // file could wrap round and the buffer come out smaller than what is written into it.
    const std::uint64_t size = ReadUInt32(file, 4);
    const std::uint64_t headSize = ReadUInt16(file, 8);
    const std::uint64_t tailSize = ReadUInt32(file, 10);
    static_assert(std::is_same_v<decltype(headSize + tailSize), std::uint64_t>,
                  "the packed sizes must be added in 64 bits");
    static_assert(std::is_same_v<decltype(size - headSize - tailSize), std::uint64_t>,
                  "the packed sizes must be subtracted in 64 bits");
    const std::uint64_t rest = file.size() - PrefixSize;
    // Where the WebP starts; CheckLayout below checks the whole layout once the picture's size is known.
    if (headSize < Ozt::HeaderSize || headSize + tailSize > rest)
    {
        return false;
    }
    const std::uint8_t* head = file.data() + PrefixSize;
    const std::uint8_t* tail = head + static_cast<std::size_t>(headSize);
    const std::uint8_t* webp = tail + static_cast<std::size_t>(tailSize);
    const auto webpSize = static_cast<std::size_t>(rest - headSize - tailSize);

    int width = 0;
    int height = 0;
    if (!WebPGetInfo(webp, webpSize, &width, &height) || width <= 0 || height <= 0 || width > MaximumWidth ||
        height > MaximumHeight)
    {
        return false;
    }
    // The picture must be the one the OZT header describes, 32 bits a pixel.
    if (ReadUInt16(file, PrefixSize + WidthOffset) != width || ReadUInt16(file, PrefixSize + HeightOffset) != height ||
        file[PrefixSize + BitsOffset] != 32)
    {
        return false;
    }
    const std::uint64_t pixelBytes = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4u;
    if (!CheckLayout(file.size(), size, headSize, tailSize, pixelBytes))
    {
        return false;
    }

    try
    {
        out.resize(static_cast<std::size_t>(size));
    }
    catch (const std::bad_alloc&)
    {
        out.clear();
        return false;
    }
    catch (const std::length_error&)
    {
        out.clear();
        return false;
    }
    std::memcpy(out.data(), head, static_cast<std::size_t>(headSize));
    // The OZT keeps its pixels as BGRA in file order, as hd_pack.py packed them.
    if (WebPDecodeBGRAInto(webp, webpSize, out.data() + headSize, static_cast<std::size_t>(pixelBytes), width * 4) ==
        nullptr)
    {
        out.clear();
        return false;
    }
    std::memcpy(out.data() + headSize + pixelBytes, tail, static_cast<std::size_t>(tailSize));
    return true;
#else
    return false;
#endif
}
} // namespace Render::Textures::PackedOzt
