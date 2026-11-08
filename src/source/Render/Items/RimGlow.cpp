#include "stdafx.h"
#include "Render/Items/RimGlow.h"

#include "Render/Textures/HdTextures.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Render::Items::RimGlow
{
namespace
{
GLuint g_texture = BITMAP_UNKNOWN;
bool g_made = false;

float SmoothStep(float edge0, float edge1, float x)
{
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// u: 0 where the surface faces the camera, 1 at the outline; v: the band's phase (repeats).
float Value(float u, float v)
{
    const float outline = std::pow(SmoothStep(0.45f, 1.0f, u), 1.4f);
    const float fromBand = (v - 0.5f) / 0.035f;
    const float band = 0.7f * std::exp(-fromBand * fromBand) * (0.4f + 0.6f * u);
    return std::min(1.0f, outline + band);
}

void Paint(std::uint8_t* pixels, std::size_t count, int components)
{
    // Effect\Chrome01 is square (64 px, 256 px as its HD copy).
    const int side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(count))));
    if (side <= 0 || static_cast<std::size_t>(side) * side != count)
        return;
    for (int y = 0; y < side; ++y)
    {
        for (int x = 0; x < side; ++x)
        {
            const float value = Value((x + 0.5f) / side, (y + 0.5f) / side);
            const auto byte = static_cast<std::uint8_t>(std::lround(value * 255.0f));
            std::uint8_t* pixel = pixels + (static_cast<std::size_t>(y) * side + x) * components;
            pixel[0] = byte;
            pixel[1] = byte;
            pixel[2] = byte;
            if (components == 4)
                pixel[3] = 255;
        }
    }
}
} // namespace

bool Enabled()
{
    return Render::Textures::Hd::Enabled();
}

GLuint Texture()
{
    if (!g_made)
    {
        g_made = true;
        const BITMAP_t* chrome = Bitmaps.FindTexture(BITMAP_CHROME);
        if (chrome != nullptr)
        {
            const CGlobalBitmap::PixelChange paint = [](std::uint8_t* pixels, std::size_t count, int components) {
                Paint(pixels, count, components);
            };
            g_texture = Bitmaps.LoadChangedImage(chrome->FileName, std::wstring(chrome->FileName) + L"|rimglow",
                                                 GL_LINEAR, GL_REPEAT, paint);
        }
    }
    return g_texture == BITMAP_UNKNOWN ? static_cast<GLuint>(BITMAP_CHROME) : g_texture;
}

float Strength(int level)
{
    if (level < 7)
        return 0.0f;
    if (level < 9)
        return 0.55f;
    if (level < 11)
        return 0.7f;
    if (level < 13)
        return 0.85f;
    return 1.0f;
}

} // namespace Render::Items::RimGlow
