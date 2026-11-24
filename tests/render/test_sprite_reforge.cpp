#include "doctest.h"

#include "Render/Textures/SpriteReforge.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

using Render::Textures::Reforge::Apply;
using Render::Textures::Reforge::ParsePreset;
using Render::Textures::Reforge::Preset;

namespace
{
using Pixels = std::vector<std::uint8_t>;

struct Size
{
    int width = 0;
    int height = 0;
};

constexpr std::array<Preset, 9> ChangingPresets{Preset::Metal, Preset::MetalFlat, Preset::MetalBright,
                                                Preset::Cell,  Preset::Stone,     Preset::Socket,
                                                Preset::Gild,  Preset::Frame,     Preset::FrameFlat};

Pixels Uniform(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    Pixels pixels;
    for (int i = 0; i < width * height; ++i)
        pixels.insert(pixels.end(), {r, g, b, a});
    return pixels;
}

// Grey relief with a light diagonal band, a coloured corner and varied alpha: every preset changes something.
Pixels Pattern(int width, int height)
{
    Pixels pixels;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const int band = std::abs(x - y) < 3 ? 120 : 0;
            const auto grey = static_cast<std::uint8_t>(std::min(255, 40 + 4 * x + 3 * y + band));
            const bool coloured = x < 4 && y < 4;
            const auto alpha = static_cast<std::uint8_t>((x * 37 + y * 11) % 256);
            if (coloured)
                pixels.insert(pixels.end(), {40, 200, 40, alpha});
            else
                pixels.insert(pixels.end(), {grey, grey, grey, alpha});
        }
    }
    return pixels;
}

Pixels Forged(Pixels pixels, Preset preset, int width, int height, int components = 4, int scale = 1)
{
    Apply(preset, pixels.data(), width, height, components, scale);
    return pixels;
}

Pixels NearestUpscaled(const Pixels& pixels, int width, int height, int factor)
{
    Pixels result;
    for (int y = 0; y < height * factor; ++y)
    {
        for (int x = 0; x < width * factor; ++x)
        {
            const auto* pixel = &pixels[(static_cast<std::size_t>(y / factor) * width + x / factor) * 4];
            result.insert(result.end(), pixel, pixel + 4);
        }
    }
    return result;
}

// Each factor x factor block averaged into one pixel: an HD copy as it shows at the original's size.
Pixels BoxDownscaled(const Pixels& pixels, int width, int height, int factor)
{
    Pixels result;
    const int area = factor * factor;
    for (int y = 0; y < height / factor; ++y)
    {
        for (int x = 0; x < width / factor; ++x)
        {
            for (int channel = 0; channel < 4; ++channel)
            {
                int sum = 0;
                for (int dy = 0; dy < factor; ++dy)
                    for (int dx = 0; dx < factor; ++dx)
                        sum +=
                            pixels[(static_cast<std::size_t>(y * factor + dy) * width + x * factor + dx) * 4 + channel];
                result.push_back(static_cast<std::uint8_t>((sum + area / 2) / area));
            }
        }
    }
    return result;
}

int WorstDifference(const Pixels& a, const Pixels& b)
{
    int worst = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        worst = std::max(worst, std::abs(a[i] - b[i]));
    return worst;
}

void CheckColour(const Pixels& pixels, std::size_t pixel, int r, int g, int b, int tolerance)
{
    CAPTURE(pixel);
    CHECK(std::abs(pixels[pixel * 4 + 0] - r) <= tolerance);
    CHECK(std::abs(pixels[pixel * 4 + 1] - g) <= tolerance);
    CHECK(std::abs(pixels[pixel * 4 + 2] - b) <= tolerance);
}
} // namespace

TEST_CASE("a reforge preset is one of the ten names [render][textures][reforge]")
{
    CHECK(ParsePreset("none") == Preset::None);
    CHECK(ParsePreset("metal") == Preset::Metal);
    CHECK(ParsePreset("metal-flat") == Preset::MetalFlat);
    CHECK(ParsePreset("metal-bright") == Preset::MetalBright);
    CHECK(ParsePreset("cell") == Preset::Cell);
    CHECK(ParsePreset("stone") == Preset::Stone);
    CHECK(ParsePreset("socket") == Preset::Socket);
    CHECK(ParsePreset("gild") == Preset::Gild);
    CHECK(ParsePreset("frame") == Preset::Frame);
    CHECK(ParsePreset("frame-flat") == Preset::FrameFlat);
    CHECK_FALSE(ParsePreset("Metal").has_value());
    CHECK_FALSE(ParsePreset("metal ").has_value());
    CHECK_FALSE(ParsePreset("metal_flat").has_value());
    CHECK_FALSE(ParsePreset("glow").has_value());
    CHECK_FALSE(ParsePreset("").has_value());
}

TEST_CASE("the none preset leaves every byte as it was [render][textures][reforge]")
{
    const Pixels source = Pattern(16, 12);
    CHECK(Forged(source, Preset::None, 16, 12) == source);
}

TEST_CASE("no preset changes alpha or a fully transparent pixel [render][textures][reforge]")
{
    // 7 x 5 leaves partial blocks at the right and bottom edges at scale 2 and 4.
    for (const Size size : {Size{16, 12}, Size{7, 5}})
    {
        const Pixels source = Pattern(size.width, size.height);
        for (const int scale : {1, 2, 4})
        {
            for (const Preset preset : ChangingPresets)
            {
                CAPTURE(size.width);
                CAPTURE(scale);
                CAPTURE(static_cast<int>(preset));
                const Pixels forged = Forged(source, preset, size.width, size.height, 4, scale);
                CHECK(forged != source);
                for (std::size_t i = 0; i < source.size(); i += 4)
                {
                    CHECK(forged[i + 3] == source[i + 3]);
                    if (source[i + 3] == 0)
                        CHECK(std::equal(source.begin() + i, source.begin() + i + 4, forged.begin() + i));
                }
            }
        }
    }
}

TEST_CASE("metal-flat puts grey on the metal ramp [render][textures][reforge]")
{
    CheckColour(Forged(Uniform(4, 4, 150, 150, 150), Preset::MetalFlat, 4, 4), 5, 132, 104, 64, 1);
    CheckColour(Forged(Uniform(4, 4, 0, 0, 0), Preset::MetalFlat, 4, 4), 5, 5, 4, 4, 0);
    CheckColour(Forged(Uniform(4, 4, 255, 255, 255), Preset::MetalFlat, 4, 4), 5, 255, 238, 196, 0);
}

TEST_CASE("frame-flat puts grey on the light bronze ramp [render][textures][reforge]")
{
    // The window frames' ramp: 150 is a light bronze, where metal-flat gives a mid bronze (132, 104, 64).
    CheckColour(Forged(Uniform(4, 4, 150, 150, 150), Preset::FrameFlat, 4, 4), 5, 210, 178, 131, 1);
    CheckColour(Forged(Uniform(4, 4, 0, 0, 0), Preset::FrameFlat, 4, 4), 5, 5, 4, 4, 0);
    CHECK(Forged(Uniform(4, 4, 200, 40, 40), Preset::FrameFlat, 4, 4) == Uniform(4, 4, 200, 40, 40));
}

TEST_CASE("frame is metal on the light ramp: never darker, lighter in the mids [render][textures][reforge]")
{
    const Pixels source = Pattern(20, 14);
    const Pixels metal = Forged(source, Preset::Metal, 20, 14);
    const Pixels frame = Forged(source, Preset::Frame, 20, 14);
    int lighter = 0;
    for (std::size_t i = 0; i < source.size(); i += 4)
    {
        CHECK(frame[i + 3] == source[i + 3]);
        const int frameSum = frame[i] + frame[i + 1] + frame[i + 2];
        const int metalSum = metal[i] + metal[i + 1] + metal[i + 2];
        CHECK(frameSum + 3 >= metalSum);
        if (frameSum > metalSum)
            ++lighter;
    }
    CHECK(lighter > 0);
}

TEST_CASE("metal-bright lifts grey before the ramp and keeps colour [render][textures][reforge]")
{
    // 150 * 1.25 = 187.5 on the ramp: light gold, where metal-flat gives a mid bronze (132, 104, 64).
    CheckColour(Forged(Uniform(4, 4, 150, 150, 150), Preset::MetalBright, 4, 4), 5, 191, 153, 95, 1);
    CHECK(Forged(Uniform(4, 4, 200, 40, 40), Preset::MetalBright, 4, 4) == Uniform(4, 4, 200, 40, 40));
}

TEST_CASE("metal lifts flat grey by its gain before the ramp [render][textures][reforge]")
{
    // A flat field has no relief, so only the 1.03 gain moves it: 145 lands on the ramp's 150.
    CheckColour(Forged(Uniform(6, 6, 145, 145, 145), Preset::Metal, 6, 6), 14, 132, 104, 64, 1);
    CheckColour(Forged(Uniform(6, 6, 0, 0, 0), Preset::Metal, 6, 6), 14, 5, 4, 4, 0);
}

TEST_CASE("metal keeps coloured pixels and white text [render][textures][reforge]")
{
    constexpr int Size = 9;
    constexpr std::size_t Middle = Size * Size / 2;
    Pixels saturated = Uniform(Size, Size, 100, 100, 100);
    saturated[Middle * 4 + 0] = 150;
    saturated[Middle * 4 + 1] = 100;
    saturated[Middle * 4 + 2] = 108; // chroma 50
    CheckColour(Forged(saturated, Preset::Metal, Size, Size), Middle, 150, 100, 108, 0);
    CHECK(Forged(Uniform(4, 4, 200, 40, 40), Preset::Metal, 4, 4) == Uniform(4, 4, 200, 40, 40));
    CHECK(Forged(Uniform(4, 4, 250, 250, 250), Preset::Metal, 4, 4) == Uniform(4, 4, 250, 250, 250));
}

TEST_CASE("metal glows warm around its lightest pixels [render][textures][reforge]")
{
    constexpr int Size = 41;
    constexpr int LightFrom = 10;
    constexpr int LightTo = 31;
    Pixels source = Uniform(Size, Size, 60, 60, 60);
    for (int y = LightFrom; y < LightTo; ++y)
    {
        for (int x = LightFrom; x < LightTo; ++x)
            std::fill_n(source.begin() + (y * Size + x) * 4, 3, std::uint8_t{215});
    }
    const Pixels forged = Forged(source, Preset::Metal, Size, Size);
    const Pixels flat = Forged(source, Preset::MetalFlat, Size, Size);
    // Four pixels below the light square: brighter than the flat metal, and warmer.
    const std::size_t beside = static_cast<std::size_t>(LightTo + 3) * Size + Size / 2;
    CHECK(forged[beside * 4 + 0] > flat[beside * 4 + 0]);
    CHECK(forged[beside * 4 + 0] - flat[beside * 4 + 0] > forged[beside * 4 + 2] - flat[beside * 4 + 2]);
}

TEST_CASE("gild turns the light green frame gold and the green fill deep bronze [render][textures][reforge]")
{
    // The frame (luma 183) takes the metal: 183 * 1.03 on the ramp, gold.
    const Pixels gold = Forged(Uniform(4, 4, 80, 255, 80), Preset::Gild, 4, 4);
    CheckColour(gold, 5, 192, 154, 96, 1);
    CHECK(gold[0] > gold[1]);
    CHECK(gold[1] > gold[2]);
    // The fill (luma 77) turns bronze by its luma: 77 * (0.62, 0.42, 0.22).
    CheckColour(Forged(Uniform(4, 4, 30, 110, 30), Preset::Gild, 4, 4), 5, 48, 32, 17, 1);
    CHECK(Forged(Uniform(4, 4, 200, 40, 40), Preset::Gild, 4, 4) == Uniform(4, 4, 200, 40, 40));
    // Grey around the box gets the metal.
    CHECK(Forged(Uniform(4, 4, 150, 150, 150), Preset::Gild, 4, 4) ==
          Forged(Uniform(4, 4, 150, 150, 150), Preset::Metal, 4, 4));
}

TEST_CASE("stone is lit from the top, evenly across, and keeps light text [render][textures][reforge]")
{
    // Windows show the body from its left edge at 112, 190 or 222 wide, so the light has no
    // horizontal part: a row is the same across.
    constexpr int Width = 222;
    constexpr int Height = 430;
    const Pixels forged = Forged(Uniform(Width, Height, 60, 60, 60), Preset::Stone, Width, Height);
    const std::size_t top = Width / 2;
    const std::size_t bottom = static_cast<std::size_t>(Height - 1) * Width + Width / 2;
    const std::size_t left = static_cast<std::size_t>(Height / 2) * Width;
    const std::size_t middle = left + Width / 2;
    const std::size_t right = left + Width - 1;
    CHECK(forged[top * 4] > forged[bottom * 4]);
    CHECK(forged[left * 4] == forged[middle * 4]);
    CHECK(forged[right * 4] == forged[middle * 4]);
    CHECK(forged[middle * 4] < 60); // darker than the grey it was
    CHECK(Forged(Uniform(8, 8, 230, 230, 230), Preset::Stone, 8, 8) == Uniform(8, 8, 230, 230, 230));
}

TEST_CASE("socket sinks the dark background and turns the light lines to light bronze [render][textures][reforge]")
{
    const Pixels recess = Forged(Uniform(4, 4, 50, 50, 50), Preset::Socket, 4, 4);
    CheckColour(recess, 0, 28, 24, 21, 1); // 50 * 0.55 * (1, 0.88, 0.76)
    CHECK(Forged(Uniform(4, 4, 150, 150, 150), Preset::Socket, 4, 4) ==
          Forged(Uniform(4, 4, 150, 150, 150), Preset::FrameFlat, 4, 4));
}

TEST_CASE("cell brightens the lattice onto the light ramp's bronze [render][textures][reforge]")
{
    // 50 * 1.65 = 82.5 on the light ramp, even for a coloured pixel.
    CheckColour(Forged(Uniform(3, 3, 50, 50, 50), Preset::Cell, 3, 3), 4, 116, 98, 72, 1);
    const Pixels teal = Forged(Uniform(3, 3, 20, 60, 60), Preset::Cell, 3, 3);
    CHECK(teal[0] > teal[2]);
}

TEST_CASE("a preset gives the same pixels every time [render][textures][reforge]")
{
    const Pixels source = Pattern(20, 14);
    const Pixels partial = Pattern(7, 5);
    for (const Preset preset : ChangingPresets)
    {
        CAPTURE(static_cast<int>(preset));
        CHECK(Forged(source, preset, 20, 14) == Forged(source, preset, 20, 14));
        CHECK(Forged(partial, preset, 7, 5, 4, 4) == Forged(partial, preset, 7, 5, 4, 4));
    }
}

TEST_CASE("an HD copy at scale 4 looks like the original at scale 1 [render][textures][reforge]")
{
    constexpr int Width = 24;
    constexpr int Height = 16;
    constexpr int Factor = 4;
    constexpr int Tolerance = 6;
    Pixels source = Pattern(Width, Height);
    for (std::size_t i = 3; i < source.size(); i += 4)
        source[i] = 255;
    const Pixels upscaled = NearestUpscaled(source, Width, Height, Factor);
    for (const Preset preset : ChangingPresets)
    {
        CAPTURE(static_cast<int>(preset));
        const Pixels original = Forged(source, preset, Width, Height, 4, 1);
        const Pixels hd = Forged(upscaled, preset, Width * Factor, Height * Factor, 4, Factor);
        CHECK(WorstDifference(BoxDownscaled(hd, Width * Factor, Height * Factor, Factor), original) <= Tolerance);
    }
    // The comparison sees a radius that does not grow with the scale: the presets that blur, forged as scale 1.
    for (const Preset preset : {Preset::Metal, Preset::Stone})
    {
        CAPTURE(static_cast<int>(preset));
        const Pixels original = Forged(source, preset, Width, Height, 4, 1);
        const Pixels unscaled = Forged(upscaled, preset, Width * Factor, Height * Factor, 4, 1);
        CHECK(WorstDifference(BoxDownscaled(unscaled, Width * Factor, Height * Factor, Factor), original) > Tolerance);
    }
}

TEST_CASE("tiny and fully transparent sprites come through unchanged [render][textures][reforge]")
{
    for (const Preset preset : ChangingPresets)
    {
        CAPTURE(static_cast<int>(preset));
        const Pixels one = Uniform(1, 1, 90, 30, 200, 0);
        CHECK(Forged(one, preset, 1, 1) == one);
        const Pixels four = Uniform(2, 2, 255, 255, 255, 0);
        CHECK(Forged(four, preset, 2, 2) == four);
        Pixels opaque = Uniform(1, 1, 120, 120, 120);
        Apply(preset, opaque.data(), 1, 1, 4, 4);
        CHECK(opaque[3] == 255);
        Apply(preset, nullptr, 4, 4, 4, 1);
        Apply(preset, opaque.data(), 0, 1, 4, 1);
        Apply(preset, opaque.data(), 1, 1, 2, 1);
    }
}

TEST_CASE("RGB pixels forge like opaque RGBA pixels [render][textures][reforge]")
{
    constexpr int Width = 12;
    constexpr int Height = 10;
    Pixels rgba = Pattern(Width, Height);
    Pixels rgb;
    for (std::size_t i = 0; i < rgba.size(); i += 4)
    {
        rgba[i + 3] = 255;
        rgb.insert(rgb.end(), rgba.begin() + i, rgba.begin() + i + 3);
    }
    for (const Preset preset : ChangingPresets)
    {
        CAPTURE(static_cast<int>(preset));
        const Pixels fromRgba = Forged(rgba, preset, Width, Height, 4);
        const Pixels fromRgb = Forged(rgb, preset, Width, Height, 3);
        for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(Width * Height); ++pixel)
        {
            for (std::size_t channel = 0; channel < 3; ++channel)
                CHECK(fromRgb[pixel * 3 + channel] == fromRgba[pixel * 4 + channel]);
        }
    }
}
