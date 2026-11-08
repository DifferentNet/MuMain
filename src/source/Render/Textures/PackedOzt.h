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

// Whether the sizes of a packed file add up: a file of `fileSize` bytes holds the prefix, a head
// of `headSize` (at least the OZT header) and a tail of `tailSize`, and the original of `size`
// bytes is exactly head + `pixelBytes` + tail. All in 64 bits, so no sum of two 32-bit sizes from
// a broken file wraps round (on x86 a wrapped sum would make the buffer smaller than its writes).
bool CheckLayout(std::uint64_t fileSize, std::uint64_t size, std::uint64_t headSize, std::uint64_t tailSize,
                 std::uint64_t pixelBytes);

// True when the file starts with the packed marker.
bool IsPacked(std::span<const std::uint8_t> file);

// The original OZT of a packed file. False (and out left empty) when the file
// is broken, its WebP can't be decoded, or the sizes don't add up, or when the
// build has no WebP decoder.
bool Unpack(std::span<const std::uint8_t> file, std::vector<std::uint8_t>& out);
} // namespace Render::Textures::PackedOzt
