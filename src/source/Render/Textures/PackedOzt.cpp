#include "PackedOzt.h"

#include "OztFile.h"

#include <cstring>

#if defined(MU_HAS_WEBP)
#include <webp/decode.h>
#endif

namespace Render::Textures::PackedOzt
{
namespace
{
constexpr std::uint8_t Magic[4] = {'M', 'U', 'W', '1'};

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
    const std::size_t size = ReadUInt32(file, 4);
    const std::size_t headSize = ReadUInt16(file, 8);
    const std::size_t tailSize = ReadUInt32(file, 10);
    if (headSize < Ozt::HeaderSize || file.size() - PrefixSize < headSize + tailSize)
    {
        return false;
    }
    const std::uint8_t* head = file.data() + PrefixSize;
    const std::uint8_t* tail = head + headSize;
    const std::uint8_t* webp = tail + tailSize;
    const std::size_t webpSize = file.size() - PrefixSize - headSize - tailSize;

    int width = 0;
    int height = 0;
    if (!WebPGetInfo(webp, webpSize, &width, &height) || width <= 0 || height <= 0)
    {
        return false;
    }
    const std::size_t pixelBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if (headSize + pixelBytes + tailSize != size)
    {
        return false;
    }

    out.resize(size);
    std::memcpy(out.data(), head, headSize);
    // The OZT keeps its pixels as BGRA in file order, as hd_pack.py packed them.
    if (WebPDecodeBGRAInto(webp, webpSize, out.data() + headSize, pixelBytes, width * 4) == nullptr)
    {
        out.clear();
        return false;
    }
    std::memcpy(out.data() + headSize + pixelBytes, tail, tailSize);
    return true;
#else
    return false;
#endif
}
} // namespace Render::Textures::PackedOzt
