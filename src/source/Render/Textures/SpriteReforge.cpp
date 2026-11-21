#include "Render/Textures/SpriteReforge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

// For a per-pixel function called from two places, which MSVC would call rather than inline: the
// call costs about a sixth of the forge.
#if defined(_MSC_VER)
#define SPRITE_REFORGE_INLINE __forceinline
#else
#define SPRITE_REFORGE_INLINE inline __attribute__((always_inline))
#endif

// The presets port side-session-notes/ui-refresh/tools/mock_a.py and mock_hud.py (main repo), which
// recoloured screenshots taken at 2.25 screen pixels per pixel of the art; their blur radii are given
// here in pixels of the original art. The blurred terms (metal's relief and glow, stone's softening)
// are worked out on the original art's grid, one value per scale x scale block of an HD copy, and read
// back at each texture pixel bilinearly: an HD copy looks like its original, and the work at texture
// size is one pass over the pixels with no planes of that size.
namespace Render::Textures::Reforge
{
namespace
{
struct Rgb
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct RampStop
{
    float at = 0.0f;
    Rgb colour;
};

// A smoothstep's edges: 0 at or below `from`, 1 at or above `from + 1 / perUnit`.
struct Step
{
    float from = 0.0f;
    float perUnit = 0.0f;
};

constexpr Step MakeStep(float from, float to)
{
    return {from, 1.0f / (to - from)};
}

constexpr float MaxChannel = 255.0f;
constexpr float LumaRed = 0.299f;
constexpr float LumaGreen = 0.587f;
constexpr float LumaBlue = 0.114f;

// Dark iron in the mids, antique gold where the original's curls were light (by luma).
constexpr std::array<RampStop, 8> MetalRamp{{{0.0f, {5.0f, 4.0f, 4.0f}},
                                             {40.0f, {22.0f, 19.0f, 17.0f}},
                                             {80.0f, {44.0f, 38.0f, 33.0f}},
                                             {115.0f, {78.0f, 64.0f, 46.0f}},
                                             {150.0f, {132.0f, 104.0f, 64.0f}},
                                             {185.0f, {188.0f, 150.0f, 92.0f}},
                                             {220.0f, {232.0f, 198.0f, 132.0f}},
                                             {255.0f, {255.0f, 238.0f, 196.0f}}}};

// Warm dark stone for window bodies (by luma).
constexpr std::array<RampStop, 7> StoneRamp{{{0.0f, {5.0f, 4.0f, 4.0f}},
                                             {25.0f, {14.0f, 11.0f, 10.0f}},
                                             {45.0f, {24.0f, 19.0f, 16.0f}},
                                             {70.0f, {40.0f, 32.0f, 26.0f}},
                                             {100.0f, {62.0f, 50.0f, 40.0f}},
                                             {140.0f, {96.0f, 80.0f, 62.0f}},
                                             {255.0f, {200.0f, 180.0f, 150.0f}}}};

// Grey and near-grey pixels (chroma below the first value) take the new colours; coloured ones,
// such as gems, bars and icons, keep theirs.
constexpr Step GreyChroma = MakeStep(18.0f, 42.0f);

// metal: local contrast against the surround (deeper slots, crisper rims), a slight gain.
constexpr float MetalReliefRadius = 2.2f;
constexpr float MetalRelief = 0.55f;
constexpr float MetalGain = 1.03f;
// metal: white text painted into a sprite stays white.
constexpr Step TextLuma = MakeStep(200.0f, 232.0f);
constexpr Step TextChroma = MakeStep(20.0f, 40.0f);
// metal: a warm glow around the lightest recoloured pixels, screen-blended.
constexpr Step GlowLuma = MakeStep(185.0f, 235.0f);
// The glow's colour, 0-1 per channel.
constexpr Rgb GlowColour{255.0f / MaxChannel, 160.0f / MaxChannel, 60.0f / MaxChannel};
constexpr float GlowStrength = 0.42f;
constexpr float GlowNearRadius = 1.8f;
constexpr float GlowNearShare = 0.55f;
constexpr float GlowFarRadius = 6.0f;
constexpr float GlowFarShare = 0.65f;

// metal-bright: the HUD's menu buttons have light grey icons on dark grey; lifted before the ramp,
// the icons land on light gold and stay as readable as the original's.
constexpr float MetalBrightGain = 1.25f;

// cell: the lattice is dim teal; brightened, it lands on the ramp's dark bronze.
constexpr float CellGain = 1.65f;

// stone: a little blur takes the JPEG noise off, the ramp darkens, bright pixels (text) stay.
constexpr float StoneBlurRadius = 0.7f;
constexpr float StoneBlurShare = 0.45f;
constexpr float StoneDarken = 0.85f;
constexpr Step StoneKeepLuma = MakeStep(120.0f, 165.0f);
// stone: light from the top, falling off over a window body of the inventory's height. Windows show
// the sprite from its top-left corner at 112, 190 or 222 wide, so the light has no horizontal term.
constexpr float StoneBodyHeight = 430.0f;
constexpr float StoneLightTop = 1.18f;
constexpr float StoneLightFall = 0.38f;
constexpr Step StoneLitLuma = MakeStep(150.0f, 190.0f);

// socket: the dark grey background becomes a darker warm recess, the brighter grey lines metal.
constexpr Step SocketChroma = MakeStep(14.0f, 30.0f);
constexpr Step SocketLineLuma = MakeStep(80.0f, 125.0f);
constexpr float SocketShade = 0.55f;
constexpr Rgb SocketTint{1.0f, 0.88f, 0.76f};

// gild: the acid green of the current-skill box. Its light frame takes the metal's gold (with the
// relief and the glow); its darker fill turns deep bronze, so a skill icon on it stands out. The chroma
// step starts low enough to take the fill's dark green edge as well.
constexpr Step GildGreenOverBlue = MakeStep(10.0f, 30.0f);
constexpr Step GildGreenOverRed = MakeStep(-5.0f, 10.0f);
constexpr Step GildChroma = MakeStep(12.0f, 36.0f);
constexpr Step GildFrameLuma = MakeStep(140.0f, 175.0f);
constexpr Rgb GildFill{0.62f, 0.42f, 0.22f};

// Below this blurred alpha a block has no opaque neighbours to average.
constexpr float MinCoverage = 1.0e-4f;

constexpr std::array<std::pair<std::string_view, Preset>, 8> PresetNames{{{"none", Preset::None},
                                                                          {"metal", Preset::Metal},
                                                                          {"metal-flat", Preset::MetalFlat},
                                                                          {"metal-bright", Preset::MetalBright},
                                                                          {"cell", Preset::Cell},
                                                                          {"stone", Preset::Stone},
                                                                          {"socket", Preset::Socket},
                                                                          {"gild", Preset::Gild}}};

// The caller's pixels: rows top-down, 3 or 4 components, straight alpha.
struct Sprite
{
    std::uint8_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    int components = 0;
    int scale = 1;

    [[nodiscard]] std::uint8_t* Row(int y) const
    {
        return pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * components;
    }
};

// A plane on the original art's grid: one value per scale x scale block of the sprite, rows top-down.
struct Grid
{
    int width = 0;
    int height = 0;
    std::vector<float> values;
};

// The sprite's blocks averaged: colour weighted by alpha (a transparent pixel's colour counts for
// nothing; black for a block with no opaque pixel) and alpha 0-1.
struct Blocks
{
    int width = 0;
    int height = 0;
    std::vector<Rgb> colour;
    std::vector<float> alpha;
    bool opaque = true; // every pixel fully opaque
};

// Where a texture pixel's centre falls between two block centres: `index` and the next one, `weight`
// of the way.
struct Between
{
    int index = 0;
    float weight = 0.0f;
};

// Reads a grid back at the sprite's pixels, bilinearly between block centres; at scale 1 it gives
// the grid's own values.
struct GridSampler
{
    const Grid& grid;
    int scale = 1;
    std::vector<Between> columns;
    std::vector<float> row; // the grid between two block rows, the last value repeated
};

// By value: std::max and std::min return references, which MSVC keeps on the stack on the per-pixel
// path.
constexpr float Larger(float a, float b)
{
    return a > b ? a : b;
}

constexpr float Smaller(float a, float b)
{
    return a < b ? a : b;
}

constexpr float Clamp(float x, float low, float high)
{
    return Smaller(Larger(x, low), high);
}

constexpr float Smooth(const Step& step, float x)
{
    const float t = Clamp((x - step.from) * step.perUnit, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float Luma(const Rgb& c)
{
    return LumaRed * c.r + LumaGreen * c.g + LumaBlue * c.b;
}

float Chroma(const Rgb& c)
{
    return Larger(Larger(c.r, c.g), c.b) - Smaller(Smaller(c.r, c.g), c.b);
}

constexpr Rgb Mix(const Rgb& from, const Rgb& to, float t)
{
    return {from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t, from.b + (to.b - from.b) * t};
}

Rgb Scaled(const Rgb& c, float factor)
{
    return {c.r * factor, c.g * factor, c.b * factor};
}

Rgb Scaled(const Rgb& c, const Rgb& factor)
{
    return {c.r * factor.r, c.g * factor.g, c.b * factor.b};
}

Rgb Clamped(const Rgb& c)
{
    return {Clamp(c.r, 0.0f, MaxChannel), Clamp(c.g, 0.0f, MaxChannel), Clamp(c.b, 0.0f, MaxChannel)};
}

template <std::size_t Count> constexpr Rgb Ramp(const std::array<RampStop, Count>& stops, float x)
{
    if (x <= stops.front().at)
        return stops.front().colour;
    for (std::size_t i = 1; i < Count; ++i)
    {
        if (x <= stops[i].at)
        {
            const float t = (x - stops[i - 1].at) / (stops[i].at - stops[i - 1].at);
            return Mix(stops[i - 1].colour, stops[i].colour, t);
        }
    }
    return stops.back().colour;
}

// A ramp looked up per luma step: its stops sit on whole steps, so blending two neighbouring entries
// gives the ramp itself.
class RampTable
{
public:
    template <std::size_t Count> constexpr explicit RampTable(const std::array<RampStop, Count>& stops)
    {
        for (std::size_t i = 0; i < Steps; ++i)
            m_entries[i] = Ramp(stops, static_cast<float>(i));
        m_entries[Steps] = m_entries[Steps - 1];
    }

    [[nodiscard]] Rgb operator()(float x) const
    {
        const float at = Clamp(x, 0.0f, MaxChannel);
        const int step = static_cast<int>(at);
        return Mix(m_entries[step], m_entries[step + 1], at - static_cast<float>(step));
    }

private:
    static constexpr std::size_t Steps = 256;
    std::array<Rgb, Steps + 1> m_entries{};
};

constexpr RampTable MetalColours(MetalRamp);
constexpr RampTable StoneColours(StoneRamp);

// How much a pixel takes the new colours by its chroma: 1 for grey, 0 for coloured.
constexpr float GreyWeight(float chroma)
{
    return 1.0f - Smooth(GreyChroma, chroma);
}

// How much a pixel is the current-skill box's acid green.
float GreenWeight(const Rgb& c, float chroma)
{
    return Smooth(GildGreenOverBlue, c.g - c.b) * Smooth(GildGreenOverRed, c.g - c.r) * Smooth(GildChroma, chroma);
}

std::uint8_t ToByte(float value)
{
    return static_cast<std::uint8_t>(Clamp(value, 0.0f, MaxChannel) + 0.5f);
}

Rgb ColourOf(const std::uint8_t* pixel)
{
    return {static_cast<float>(pixel[0]), static_cast<float>(pixel[1]), static_cast<float>(pixel[2])};
}

bool Transparent(const std::uint8_t* pixel, int components)
{
    return components == 4 && pixel[3] == 0;
}

void Put(std::uint8_t* pixel, const Rgb& c)
{
    pixel[0] = ToByte(c.r);
    pixel[1] = ToByte(c.g);
    pixel[2] = ToByte(c.b);
}

// The fractional box radius whose three passes give a Gaussian of this standard deviation (as
// Pillow's GaussianBlur, which the mock-ups used).
double BoxRadius(double sigma)
{
    constexpr double Passes = 3.0;
    const double variance = sigma * sigma / Passes;
    const double length = std::sqrt(12.0 * variance + 1.0);
    const double whole = std::floor((length - 1.0) / 2.0);
    const double part = (2.0 * whole + 1.0) * (whole * (whole + 1.0) - 3.0 * variance) /
                        (6.0 * (variance - (whole + 1.0) * (whole + 1.0)));
    return whole + part;
}

// A box of fractional radius: the whole box's values weigh `inner` each, the two values just outside
// it `edge` each.
struct Box
{
    int whole = 0;
    double inner = 0.0;
    double edge = 0.0;
};

Box MakeBox(double radius)
{
    const int whole = static_cast<int>(radius);
    const double inner = 1.0 / (2.0 * radius + 1.0);
    return {whole, inner, (radius - whole) * inner};
}

// One box pass over `count` consecutive values, the end values repeated past the ends. `padded` is a
// buffer reused from line to line.
void BoxLine(float* values, int count, const Box& box, std::vector<float>& padded)
{
    const int whole = box.whole;
    const int pad = whole + 1;
    padded.resize(static_cast<std::size_t>(count) + 2u * static_cast<std::size_t>(pad));
    std::fill_n(padded.begin(), pad, values[0]);
    std::copy_n(values, count, padded.begin() + pad);
    std::fill_n(padded.begin() + pad + count, pad, values[count - 1]);
    double sum = 0.0; // the whole box around the current value
    for (int k = pad - whole; k <= pad + whole; ++k)
        sum += padded[k];
    for (int x = 0, centre = pad; x < count; ++x, ++centre)
    {
        const double outside = static_cast<double>(padded[centre - whole - 1]) + padded[centre + whole + 1];
        values[x] = static_cast<float>(sum * box.inner + outside * box.edge);
        sum += static_cast<double>(padded[centre + whole + 1]) - padded[centre - whole];
    }
}

// One box pass down all columns of a plane at once, from `from` into `to`, the end rows repeated past
// the ends: BoxLine with a running sum per column, so the inner loop runs along a row.
void BoxColumns(const std::vector<float>& from, std::vector<float>& to, int width, int height, const Box& box,
                std::vector<double>& sums)
{
    const auto row = [&](int y)
    { return from.data() + static_cast<std::size_t>(std::clamp(y, 0, height - 1)) * static_cast<std::size_t>(width); };
    sums.assign(static_cast<std::size_t>(width), 0.0);
    for (int k = -box.whole; k <= box.whole; ++k)
    {
        const float* values = row(k);
        for (int x = 0; x < width; ++x)
            sums[x] += values[x];
    }
    for (int y = 0; y < height; ++y)
    {
        const float* before = row(y - box.whole - 1);
        const float* leaving = row(y - box.whole);
        const float* after = row(y + box.whole + 1);
        float* out = to.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
        for (int x = 0; x < width; ++x)
        {
            out[x] = static_cast<float>(sums[x] * box.inner + (static_cast<double>(before[x]) + after[x]) * box.edge);
            sums[x] += static_cast<double>(after[x]) - leaving[x];
        }
    }
}

// A Gaussian blur of a plane, `sigma` in its own pixels, edges repeated: three box passes along the
// rows, then three down the columns.
std::vector<float> Blur(std::vector<float> values, int width, int height, float sigma)
{
    constexpr int Passes = 3;
    if (sigma <= 0.0f)
        return values;
    const Box box = MakeBox(BoxRadius(sigma));
    std::vector<float> padded;
    for (int y = 0; y < height; ++y)
    {
        for (int pass = 0; pass < Passes; ++pass)
            BoxLine(values.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width), width, box, padded);
    }
    std::vector<float> other(values.size());
    std::vector<double> sums;
    for (int pass = 0; pass < Passes; ++pass)
    {
        BoxColumns(values, other, width, height, box, sums);
        values.swap(other);
    }
    return values;
}

// A block's sums in whole numbers: each channel times alpha, and alpha, alpha 0-255.
struct BlockSums
{
    std::uint32_t r = 0;
    std::uint32_t g = 0;
    std::uint32_t b = 0;
    std::uint32_t alpha = 0;
};

// Adds one texture row to its block row's sums. Returns whether every pixel of the row is fully
// opaque.
bool AddRow(const Sprite& sprite, int y, BlockSums* sums)
{
    constexpr std::uint32_t Opaque = 255;
    std::uint32_t allAlpha = Opaque; // the alphas' bits and-ed: 255 only if every alpha is
    const std::uint8_t* pixel = sprite.Row(y);
    for (int x = 0; x < sprite.width; ++sums)
    {
        BlockSums block;
        const int end = std::min(x + sprite.scale, sprite.width);
        for (; x < end; ++x, pixel += sprite.components)
        {
            const std::uint32_t alpha = sprite.components == 4 ? pixel[3] : Opaque;
            allAlpha &= alpha;
            block.r += pixel[0] * alpha;
            block.g += pixel[1] * alpha;
            block.b += pixel[2] * alpha;
            block.alpha += alpha;
        }
        sums->r += block.r;
        sums->g += block.g;
        sums->b += block.b;
        sums->alpha += block.alpha;
    }
    return allAlpha == Opaque;
}

int BlockSpan(int block, int scale, int size)
{
    return std::min(scale, size - block * scale);
}

Blocks Average(const Sprite& sprite)
{
    const int scale = sprite.scale;
    Blocks blocks;
    blocks.width = (sprite.width + scale - 1) / scale;
    blocks.height = (sprite.height + scale - 1) / scale;
    const std::size_t count = static_cast<std::size_t>(blocks.width) * static_cast<std::size_t>(blocks.height);
    blocks.colour.resize(count);
    blocks.alpha.resize(count);
    std::vector<BlockSums> sums(static_cast<std::size_t>(blocks.width));
    for (int by = 0; by < blocks.height; ++by)
    {
        std::fill(sums.begin(), sums.end(), BlockSums{});
        const int rows = BlockSpan(by, scale, sprite.height);
        for (int y = by * scale; y < by * scale + rows; ++y)
            blocks.opaque = AddRow(sprite, y, sums.data()) && blocks.opaque;
        for (int bx = 0; bx < blocks.width; ++bx)
        {
            const BlockSums& block = sums[bx];
            const std::size_t i = static_cast<std::size_t>(by) * blocks.width + bx;
            if (block.alpha > 0)
            {
                const float perAlpha = 1.0f / static_cast<float>(block.alpha);
                blocks.colour[i] = {static_cast<float>(block.r) * perAlpha, static_cast<float>(block.g) * perAlpha,
                                    static_cast<float>(block.b) * perAlpha};
            }
            const int pixels = BlockSpan(bx, scale, sprite.width) * rows;
            blocks.alpha[i] = static_cast<float>(block.alpha) / (MaxChannel * static_cast<float>(pixels));
        }
    }
    return blocks;
}

// The luma of each block's opaque surround: the blur of luma weighted by alpha over the blur of
// alpha, so transparent pixels around a sprite don't darken its edges.
Grid Surround(const Blocks& blocks, float sigma)
{
    std::vector<float> weighted(blocks.colour.size());
    for (std::size_t i = 0; i < weighted.size(); ++i)
        weighted[i] = Luma(blocks.colour[i]) * blocks.alpha[i];
    weighted = Blur(std::move(weighted), blocks.width, blocks.height, sigma);
    if (blocks.opaque)
        return {blocks.width, blocks.height, std::move(weighted)};
    const std::vector<float> coverage = Blur(blocks.alpha, blocks.width, blocks.height, sigma);
    for (std::size_t i = 0; i < weighted.size(); ++i)
        weighted[i] = coverage[i] > MinCoverage ? weighted[i] / coverage[i] : Luma(blocks.colour[i]);
    return {blocks.width, blocks.height, std::move(weighted)};
}

Between Locate(int pixel, int scale, int blocks)
{
    const float at = (static_cast<float>(pixel) + 0.5f) / static_cast<float>(scale) - 0.5f;
    if (at <= 0.0f)
        return {};
    const int index = std::min(static_cast<int>(at), blocks - 1);
    if (index == blocks - 1)
        return {index, 0.0f};
    return {index, at - static_cast<float>(index)};
}

GridSampler MakeSampler(const Grid& grid, int spriteWidth, int scale)
{
    GridSampler sampler{grid, scale, std::vector<Between>(static_cast<std::size_t>(spriteWidth)),
                        std::vector<float>(static_cast<std::size_t>(grid.width) + 1u)};
    for (int x = 0; x < spriteWidth; ++x)
        sampler.columns[x] = Locate(x, scale, grid.width);
    return sampler;
}

// Prepares the sampler for texture row `y`.
void SampleRow(GridSampler& sampler, int y)
{
    const Grid& grid = sampler.grid;
    const Between between = Locate(y, sampler.scale, grid.height);
    const float* upper = grid.values.data() + static_cast<std::size_t>(between.index) * grid.width;
    const float* lower = grid.values.data() + static_cast<std::size_t>(std::min(between.index + 1, grid.height - 1)) *
                                                  static_cast<std::size_t>(grid.width);
    for (int x = 0; x < grid.width; ++x)
        sampler.row[x] = upper[x] + (lower[x] - upper[x]) * between.weight;
    sampler.row[grid.width] = sampler.row[grid.width - 1];
}

// The grid at texture pixel `x` of the prepared row.
float SampleAt(const GridSampler& sampler, int x)
{
    const Between& between = sampler.columns[x];
    const float left = sampler.row[between.index];
    return left + (sampler.row[between.index + 1] - left) * between.weight;
}

// Calls `recolour(colour, x)` for each pixel of row `y` that is not fully transparent and stores
// what it returns.
template <typename Recolour> void ForEachInRow(const Sprite& sprite, int y, Recolour&& recolour)
{
    std::uint8_t* pixel = sprite.Row(y);
    for (int x = 0; x < sprite.width; ++x, pixel += sprite.components)
    {
        if (!Transparent(pixel, sprite.components))
            Put(pixel, recolour(ColourOf(pixel), x));
    }
}

template <typename Recolour> void ForEachPixel(const Sprite& sprite, Recolour&& recolour)
{
    for (int y = 0; y < sprite.height; ++y)
        ForEachInRow(sprite, y, [&recolour](const Rgb& colour, int) { return recolour(colour); });
}

// What a pixel's chroma does in the metal look: how much of it takes the metal colours, and how
// much of it can be white text.
struct ChromaShares
{
    float grey = 0.0f;
    float plain = 0.0f;
};

constexpr ChromaShares SharesOf(float chroma)
{
    return {GreyWeight(chroma), 1.0f - Smooth(TextChroma, chroma)};
}

// The shares of each whole-number chroma, a texture pixel's, looked up.
class ChromaTable
{
public:
    constexpr ChromaTable()
    {
        for (std::size_t i = 0; i < m_entries.size(); ++i)
            m_entries[i] = SharesOf(static_cast<float>(i));
    }

    [[nodiscard]] const ChromaShares& operator()(float chroma) const
    {
        return m_entries[static_cast<int>(chroma)];
    }

private:
    std::array<ChromaShares, 256> m_entries{};
};

constexpr ChromaTable PixelShares;

// A pixel of the metal look before its glow; `shares` are its chroma's, `around` is the luma of its
// opaque surround. `share` gets how much of the pixel took the metal colours: the glow follows it.
SPRITE_REFORGE_INLINE Rgb MetalPixel(const Rgb& source, float chroma, const ChromaShares& shares, float around,
                                     bool gild, float& share)
{
    const float luma = Luma(source);
    const float relief = MetalGain * (luma + MetalRelief * (luma - around));
    const Rgb metal = MetalColours(relief);
    const float text = Smooth(TextLuma, luma) * shares.plain;
    share = shares.grey * (1.0f - text);
    const Rgb out = Mix(source, metal, share);
    if (!gild)
        return out;
    const float green = GreenWeight(source, chroma);
    const float frame = Smooth(GildFrameLuma, luma);
    const Rgb fill = Clamped(Scaled(GildFill, luma));
    share += (1.0f - share) * green * frame;
    return Mix(out, Mix(fill, metal, frame), green);
}

// Screen blend of the glow colour at `amount` (0-1).
Rgb Glowing(const Rgb& colour, float amount)
{
    return {colour.r + (MaxChannel - colour.r) * (GlowColour.r * amount),
            colour.g + (MaxChannel - colour.g) * (GlowColour.g * amount),
            colour.b + (MaxChannel - colour.b) * (GlowColour.b * amount)};
}

// How much warm glow each block gets, 0-1: the blur, near and far, of how light its metal came out.
Grid GlowAmount(const Blocks& blocks, const Grid& around, bool gild)
{
    std::vector<float> highlight(blocks.colour.size());
    for (std::size_t i = 0; i < highlight.size(); ++i)
    {
        const Rgb& colour = blocks.colour[i];
        const float chroma = Chroma(colour);
        float share = 0.0f;
        const Rgb metal = MetalPixel(colour, chroma, SharesOf(chroma), around.values[i], gild, share);
        highlight[i] = share * Smooth(GlowLuma, Luma(metal)) * blocks.alpha[i];
    }
    const std::vector<float> nearGlow = Blur(highlight, blocks.width, blocks.height, GlowNearRadius);
    std::vector<float> amount = Blur(std::move(highlight), blocks.width, blocks.height, GlowFarRadius);
    for (std::size_t i = 0; i < amount.size(); ++i)
        amount[i] = Clamp(GlowStrength * (GlowNearShare * nearGlow[i] + GlowFarShare * amount[i]), 0.0f, 1.0f);
    return {blocks.width, blocks.height, std::move(amount)};
}

void ForgeMetal(const Sprite& sprite, bool gild)
{
    const Blocks blocks = Average(sprite);
    const Grid around = Surround(blocks, MetalReliefRadius);
    const Grid glow = GlowAmount(blocks, around, gild);
    GridSampler aroundAt = MakeSampler(around, sprite.width, sprite.scale);
    GridSampler glowAt = MakeSampler(glow, sprite.width, sprite.scale);
    for (int y = 0; y < sprite.height; ++y)
    {
        SampleRow(aroundAt, y);
        SampleRow(glowAt, y);
        // Written out rather than through ForEachInRow: built with MSVC, this loop is a sixth faster.
        std::uint8_t* pixel = sprite.Row(y);
        for (int x = 0; x < sprite.width; ++x, pixel += sprite.components)
        {
            if (Transparent(pixel, sprite.components))
                continue;
            const Rgb source = ColourOf(pixel);
            const float chroma = Chroma(source);
            float share = 0.0f;
            const Rgb metal = MetalPixel(source, chroma, PixelShares(chroma), SampleAt(aroundAt, x), gild, share);
            Put(pixel, Glowing(metal, SampleAt(glowAt, x)));
        }
    }
}

Rgb MetalFlatPixel(const Rgb& source)
{
    return Mix(source, MetalColours(Luma(source)), GreyWeight(Chroma(source)));
}

Rgb MetalBrightPixel(const Rgb& source)
{
    return Mix(source, MetalColours(MetalBrightGain * Luma(source)), GreyWeight(Chroma(source)));
}

Rgb CellPixel(const Rgb& source)
{
    return MetalColours(CellGain * Luma(source));
}

// The top light of a window body at a texture row's centre: brighter at the top.
float StoneLight(int y, int scale)
{
    const float unitY = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale);
    const float down = std::clamp(unitY / StoneBodyHeight, 0.0f, 1.0f);
    return StoneLightTop - StoneLightFall * down;
}

Rgb StonePixel(const Rgb& source, float around, float light)
{
    const float luma = Luma(source);
    const float soft = luma + StoneBlurShare * (around - luma);
    const float weight = GreyWeight(Chroma(source)) * (1.0f - Smooth(StoneKeepLuma, luma));
    const Rgb stone = Mix(source, StoneColours(StoneDarken * soft), weight);
    const float lit = 1.0f + (light - 1.0f) * (1.0f - Smooth(StoneLitLuma, luma));
    return Scaled(stone, lit);
}

void ForgeStone(const Sprite& sprite)
{
    const Grid around = Surround(Average(sprite), StoneBlurRadius);
    GridSampler aroundAt = MakeSampler(around, sprite.width, sprite.scale);
    for (int y = 0; y < sprite.height; ++y)
    {
        SampleRow(aroundAt, y);
        const float light = StoneLight(y, sprite.scale);
        ForEachInRow(sprite, y,
                     [&](const Rgb& source, int x) { return StonePixel(source, SampleAt(aroundAt, x), light); });
    }
}

Rgb SocketPixel(const Rgb& source)
{
    const Rgb recessTint = Scaled(SocketTint, SocketShade);
    const float luma = Luma(source);
    const float grey = 1.0f - Smooth(SocketChroma, Chroma(source));
    const float line = Smooth(SocketLineLuma, luma);
    const Rgb socket = Mix(Scaled(source, recessTint), MetalColours(luma), line);
    return Mix(source, socket, grey);
}

void Forge(Preset preset, const Sprite& sprite)
{
    switch (preset)
    {
    case Preset::Metal:
        ForgeMetal(sprite, false);
        break;
    case Preset::Gild:
        ForgeMetal(sprite, true);
        break;
    case Preset::Stone:
        ForgeStone(sprite);
        break;
    case Preset::MetalFlat:
        ForEachPixel(sprite, [](const Rgb& source) { return MetalFlatPixel(source); });
        break;
    case Preset::MetalBright:
        ForEachPixel(sprite, [](const Rgb& source) { return MetalBrightPixel(source); });
        break;
    case Preset::Cell:
        ForEachPixel(sprite, [](const Rgb& source) { return CellPixel(source); });
        break;
    case Preset::Socket:
        ForEachPixel(sprite, [](const Rgb& source) { return SocketPixel(source); });
        break;
    case Preset::None:
        break;
    }
}
} // namespace

std::optional<Preset> ParsePreset(std::string_view name)
{
    for (const auto& [presetName, preset] : PresetNames)
    {
        if (presetName == name)
            return preset;
    }
    return std::nullopt;
}

void Apply(Preset preset, std::uint8_t* pixels, int width, int height, int components, int scale)
{
    if (preset == Preset::None || pixels == nullptr || width <= 0 || height <= 0)
        return;
    if (components != 3 && components != 4)
        return;
    Forge(preset, Sprite{pixels, width, height, components, std::max(scale, 1)});
}

} // namespace Render::Textures::Reforge
