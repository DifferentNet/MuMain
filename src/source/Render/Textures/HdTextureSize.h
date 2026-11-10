#pragma once

#include <cstdint>
#include <span>
#include <vector>

// How large the HD textures are on the GPU. The Remaster's copies are 4 times the originals on each
// side (UpscaleMarker.h), RGBA8 with every mip level: about 1.9 GiB of GPU memory, a quarter more with
// the normal maps. At half size they take a quarter of that and still hold 4 times the originals'
// texels. [Render] HdTextureSize: 0 picks by the machine at every start, 1 full, 2 half.
// Read once at start-up, like HD textures: a texture already loaded keeps its size.
namespace Render::Textures::HdSize
{

enum class Setting
{
    Auto = 0,
    Full = 1,
    Half = 2,
};

// Any saved number as a setting; an unknown one is Auto.
[[nodiscard]] constexpr Setting SettingFromConfig(int value)
{
    return value == 1 ? Setting::Full : value == 2 ? Setting::Half : Setting::Auto;
}

// Below this much system memory Auto halves the HD textures on a discrete GPU too: such a PC rarely
// has more than 2 GiB of video memory, and what does not fit there is paged through system memory.
inline constexpr int kAutoHalfBelowSystemRamMb = 12 * 1024;

// Whether the HD textures load at half size. Auto: on a GPU that shares the system memory
// (`sharedMemoryGpu`: integrated graphics or a software renderer) and on a PC with little memory
// (`systemRamMb`; 0 or less = unknown, which alone never halves).
[[nodiscard]] constexpr bool ChooseHalf(Setting setting, bool sharedMemoryGpu, int systemRamMb)
{
    if (setting != Setting::Auto)
    {
        return setting == Setting::Half;
    }
    return sharedMemoryGpu || (systemRamMb > 0 && systemRamMb < kAutoHalfBelowSystemRamMb);
}

// Set once at start-up, before the first texture loads.
void SetHalf(bool half);
[[nodiscard]] bool Half();

// By how much the loader divides each side of an image `fileScale` times the original
// (width x height as the file holds it): 2 for a copy at 4 times or more when `half` is on and both
// sides halve exactly, otherwise 1. What is uploaded then still counts as upscaled art (scale 2 or
// more: its own mip chain and samplers). An original file (scale 1) is never touched.
[[nodiscard]] constexpr int Divisor(bool half, int fileScale, int width, int height)
{
    return half && fileScale >= 4 && fileScale % 2 == 0 && width >= 2 && height >= 2 && width % 2 == 0 &&
                   height % 2 == 0
               ? 2
               : 1;
}

// An RGBA8 image (rows top to bottom, both sides even) at half size, each texel from its 2 x 2 block.
// Alpha is the block's average. With `straightAlpha` the colour is weighted by alpha, so the colour
// of transparent texels does not bleed into the edge of what is drawn (a block with no alpha at all
// keeps its plain average); without it (opaque or premultiplied pixels) the colour is the plain
// average. Empty when the sides are not even or `rgba` is too small.
[[nodiscard]] std::vector<std::uint8_t> HalveRgba(std::span<const std::uint8_t> rgba, int width, int height,
                                                  bool straightAlpha);

} // namespace Render::Textures::HdSize
