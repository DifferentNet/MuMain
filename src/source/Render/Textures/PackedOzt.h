#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Packed OZT files of the HD art (scripts/hd_pack.py): the pixels of an OZT
// as a lossless WebP picture, the header and any bytes after the pixels as
// they are. Unpacking gives the original file byte for byte, which the game
// then reads as before (OztFile.h). Layout, little endian:
//   "MUW1"  u32 original size  u16 head size  u32 tail size  head  tail  WebP
namespace Render::Textures::PackedOzt
{
inline constexpr std::size_t PrefixSize = 4 + 4 + 2 + 4;

// True when the file starts with the packed marker.
bool IsPacked(std::span<const std::uint8_t> file);

// The original OZT of a packed file. False (and out left empty) when the file
// is broken, its WebP can't be decoded, or the sizes don't add up, or when the
// build has no WebP decoder.
bool Unpack(std::span<const std::uint8_t> file, std::vector<std::uint8_t>& out);
} // namespace Render::Textures::PackedOzt
