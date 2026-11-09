#include "NormalMapFile.h"

#include "NormalMaps.h"

#include <cstring>
#include <new>

#if defined(MU_HAS_WEBP)
#include <webp/decode.h>
#endif

namespace Render::Textures::NormalMapFile
{
namespace
{
constexpr std::uint8_t Magic[4] = {'M', 'U', 'N', '2'};

int ReadUInt16(std::span<const std::uint8_t> file, std::size_t offset)
{
    return file[offset] | (file[offset + 1] << 8);
}

std::uint32_t ReadUInt32(std::span<const std::uint8_t> file, std::size_t offset)
{
    return static_cast<std::uint32_t>(ReadUInt16(file, offset)) |
           (static_cast<std::uint32_t>(ReadUInt16(file, offset + 2)) << 16);
}

#if defined(MU_HAS_WEBP)
// Decodes one grey picture of width x height and puts its red into `channel` of each RGBA texel.
bool DecodePlane(std::span<const std::uint8_t> webp, int width, int height, std::vector<std::uint8_t>& scratch,
                 std::vector<std::uint8_t>& pixels, std::size_t channel)
{
    int pictureWidth = 0;
    int pictureHeight = 0;
    if (!WebPGetInfo(webp.data(), webp.size(), &pictureWidth, &pictureHeight) || pictureWidth != width ||
        pictureHeight != height)
    {
        return false;
    }
    if (WebPDecodeRGBAInto(webp.data(), webp.size(), scratch.data(), scratch.size(), width * 4) == nullptr)
    {
        return false;
    }
    for (std::size_t i = 0; i < pixels.size(); i += 4)
    {
        pixels[i + channel] = scratch[i];
    }
    return true;
}
#endif
} // namespace

std::wstring PathFor(std::wstring_view texturePath)
{
    std::wstring path(texturePath);
    path += Suffix;
    return path;
}

std::vector<std::uint8_t> Decode(std::span<const std::uint8_t> file, int textureWidth, int textureHeight)
{
#if defined(MU_HAS_WEBP)
    if (textureWidth <= 0 || textureHeight <= 0 || file.size() <= PrefixSize ||
        std::memcmp(file.data(), Magic, sizeof(Magic)) != 0)
    {
        return {};
    }
    const int width = ReadUInt16(file, 4);
    const int height = ReadUInt16(file, 6);
    if (width != HalfSize(textureWidth) || height != HalfSize(textureHeight))
    {
        return {};
    }
    const std::size_t firstSize = ReadUInt32(file, 8);
    if (firstSize == 0 || firstSize >= file.size() - PrefixSize)
    {
        return {};
    }
    const auto first = file.subspan(PrefixSize, firstSize);
    const auto second = file.subspan(PrefixSize + firstSize);
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> scratch;
    try
    {
        pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 255);
        scratch.resize(pixels.size());
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
    // Red and green carry the tilt; blue and alpha stay 255.
    if (!DecodePlane(first, width, height, scratch, pixels, 0) ||
        !DecodePlane(second, width, height, scratch, pixels, 1))
    {
        return {};
    }
    return pixels;
#else
    (void)file;
    (void)textureWidth;
    (void)textureHeight;
    return {};
#endif
}

std::vector<std::uint8_t> Load(const std::wstring& texturePath, int textureWidth, int textureHeight,
                               const Reader& read)
{
    if (!NormalMaps::Enabled() || !NormalMaps::WantsNormalMap(texturePath) || !read)
    {
        return {};
    }
    const std::size_t maxBytes = MaxFileSize(textureWidth, textureHeight);
    std::vector<std::uint8_t> bytes;
    if (!read(PathFor(texturePath), maxBytes, bytes) || bytes.size() > maxBytes)
    {
        return {};
    }
    return Decode(bytes, textureWidth, textureHeight);
}
} // namespace Render::Textures::NormalMapFile
