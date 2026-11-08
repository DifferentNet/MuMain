#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The Remaster's prebuilt normal maps (scripts/hd_pack.py builds them with the algorithm of
// NormalMaps::Build and ships them in Data\HD). Each sits next to its texture as
// "<texture file>.nrm", at half the texture's size (rounded up), and the game only reads it:
// a missing or broken file means no relief for that texture. Layout, little endian:
//   "MUN1"  u16 width  u16 height  lossless WebP (red and green = tilt, 128 = flat)
namespace Render::Textures::NormalMapFile
{
inline constexpr std::size_t PrefixSize = 4 + 2 + 2;
inline constexpr std::wstring_view Suffix = L".nrm";

// The size of the map of a texture `textureSize` texels wide (or high).
[[nodiscard]] constexpr int HalfSize(int textureSize)
{
    return (textureSize + 1) / 2;
}

// Where the map of the texture at `texturePath` lives.
[[nodiscard]] std::wstring PathFor(std::wstring_view texturePath);

// The RGBA8 pixels (blue and alpha 255) of a map file for a texture of textureWidth x
// textureHeight; empty when the file is broken or its size is not half the texture's.
[[nodiscard]] std::vector<std::uint8_t> Decode(std::span<const std::uint8_t> file, int textureWidth,
                                               int textureHeight);

// Reads a whole file; false when it can't be opened.
using Reader = std::function<bool(const std::wstring& path, std::vector<std::uint8_t>& bytes)>;

// The map of the texture at `texturePath`: empty, without reading anything, when normal maps are
// off (the Classic client) or the texture gets none (NormalMaps::WantsNormalMap); empty too when
// its file is missing or broken. Width and height of the map are HalfSize of the texture's.
[[nodiscard]] std::vector<std::uint8_t> Load(const std::wstring& texturePath, int textureWidth, int textureHeight,
                                             const Reader& read);
} // namespace Render::Textures::NormalMapFile
