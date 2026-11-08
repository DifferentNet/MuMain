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
constexpr std::uint8_t Magic[4] = {'M', 'U', 'N', '1'};

int ReadUInt16(std::span<const std::uint8_t> file, std::size_t offset)
{
    return file[offset] | (file[offset + 1] << 8);
}
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
    const std::uint8_t* webp = file.data() + PrefixSize;
    const std::size_t webpSize = file.size() - PrefixSize;
    int pictureWidth = 0;
    int pictureHeight = 0;
    if (!WebPGetInfo(webp, webpSize, &pictureWidth, &pictureHeight) || pictureWidth != width ||
        pictureHeight != height)
    {
        return {};
    }
    std::vector<std::uint8_t> pixels;
    try
    {
        pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
    if (WebPDecodeRGBAInto(webp, webpSize, pixels.data(), pixels.size(), width * 4) == nullptr)
    {
        return {};
    }
    // Only red and green carry the tilt.
    for (std::size_t i = 0; i < pixels.size(); i += 4)
    {
        pixels[i + 2] = 255;
        pixels[i + 3] = 255;
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
    std::vector<std::uint8_t> bytes;
    if (!read(PathFor(texturePath), bytes))
    {
        return {};
    }
    return Decode(bytes, textureWidth, textureHeight);
}
} // namespace Render::Textures::NormalMapFile
