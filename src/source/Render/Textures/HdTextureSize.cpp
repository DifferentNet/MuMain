#include "Render/Textures/HdTextureSize.h"

#include <array>
#include <cstddef>

namespace Render::Textures::HdSize
{
namespace
{
bool s_half = false;
} // namespace

void SetHalf(bool half)
{
    s_half = half;
}

bool Half()
{
    return s_half;
}

std::vector<std::uint8_t> HalveRgba(std::span<const std::uint8_t> rgba, int width, int height, bool straightAlpha)
{
    if (width < 2 || height < 2 || width % 2 != 0 || height % 2 != 0 ||
        rgba.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u)
    {
        return {};
    }
    const int halfWidth = width / 2;
    const int halfHeight = height / 2;
    std::vector<std::uint8_t> half(static_cast<std::size_t>(halfWidth) * static_cast<std::size_t>(halfHeight) * 4u);
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
    for (int y = 0; y < halfHeight; ++y)
    {
        const std::uint8_t* top = rgba.data() + static_cast<std::size_t>(y) * 2u * rowBytes;
        const std::uint8_t* bottom = top + rowBytes;
        std::uint8_t* out = half.data() + static_cast<std::size_t>(y) * halfWidth * 4u;
        for (int x = 0; x < halfWidth; ++x, top += 8, bottom += 8, out += 4)
        {
            const std::array<const std::uint8_t*, 4> block{top, top + 4, bottom, bottom + 4};
            unsigned alphaSum = 0;
            for (const std::uint8_t* texel : block)
            {
                alphaSum += texel[3];
            }
            const bool weighted = straightAlpha && alphaSum > 0 && alphaSum < 4u * 255u;
            for (int channel = 0; channel < 3; ++channel)
            {
                unsigned sum = 0;
                for (const std::uint8_t* texel : block)
                {
                    sum += weighted ? texel[channel] * texel[3] : texel[channel];
                }
                const unsigned divisor = weighted ? alphaSum : 4u;
                out[channel] = static_cast<std::uint8_t>((sum + divisor / 2u) / divisor);
            }
            out[3] = static_cast<std::uint8_t>((alphaSum + 2u) / 4u);
        }
    }
    return half;
}

} // namespace Render::Textures::HdSize
