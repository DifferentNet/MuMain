#include "Render/Textures/NormalMaps.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cwctype>

namespace Render::Textures::NormalMaps
{
namespace
{
bool s_enabled = false;
bool s_runtimeBuild = false;

// How steep the relief is: the tilt per unit of brightness change per texel, for the fine detail
// and for the broad shapes (measured over `scale` texels).
constexpr float kFineStrength = 24.0f;
constexpr float kBroadStrength = 12.0f;
// Tilt never goes past this (about 64 degrees), so no texel faces sideways.
constexpr float kMaxTilt = 0.9f;
// Below this share of visible texels around it (after a blur), a texel has no height of its own.
constexpr float kMinCoverage = 1e-4f;

bool IsSeparator(wchar_t c)
{
    return c == L'\\' || c == L'/';
}

bool EqualsNoCase(std::wstring_view a, std::wstring_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::towlower(a[i]) != std::towlower(b[i]))
        {
            return false;
        }
    }
    return true;
}

bool StartsWithNoCase(std::wstring_view text, std::wstring_view prefix)
{
    return text.size() >= prefix.size() && EqualsNoCase(text.substr(0, prefix.size()), prefix);
}

bool ContainsNoCase(std::wstring_view text, std::wstring_view part)
{
    for (std::size_t i = 0; i + part.size() <= text.size(); ++i)
    {
        if (EqualsNoCase(text.substr(i, part.size()), part))
        {
            return true;
        }
    }
    return false;
}

// The part after the last separator.
std::wstring_view FileName(std::wstring_view path)
{
    std::size_t start = path.size();
    while (start > 0 && !IsSeparator(path[start - 1]))
    {
        --start;
    }
    return path.substr(start);
}

// "<prefix><digits>." at the start of `file`, as SkinClass301.OZJ or LevelClass107.OZJ.
bool IsPrefixNumberDot(std::wstring_view file, std::wstring_view prefix)
{
    if (!StartsWithNoCase(file, prefix))
    {
        return false;
    }
    std::size_t i = prefix.size();
    const std::size_t digitsStart = i;
    while (i < file.size() && file[i] >= L'0' && file[i] <= L'9')
    {
        ++i;
    }
    return i > digitsStart && i < file.size() && file[i] == L'.';
}

// A character's face (Player): the bare classes' skins (skin_*, SkinClassNNN, LevelClassNNN) and the
// heads without a helmet (*head_N, *face*). Their shading is painted in: relief built from brightness
// sinks the dark eyes and lays a band across them like a mask (ghinzuro 2026-11-11).
bool IsPlayerFace(std::wstring_view file)
{
    return StartsWithNoCase(file, L"skin_") || IsPrefixNumberDot(file, L"SkinClass") ||
           IsPrefixNumberDot(file, L"LevelClass") || ContainsNoCase(file, L"head_N.") || ContainsNoCase(file, L"face");
}

// Leaf litter strewn flat over ground, stones and walls (Lorencia Grass01/02, the Arena's Object32/33). Its HD
// picture is the original upscaled smoothly, the Classic look (scripts/hd_art.py CUTOUT_SMOOTH), and Classic has
// no relief: from brightness its tiny leaves came out half black under the side light (Lorencia fountain, 2026-11-11).
bool IsLeafLitter(std::wstring_view file)
{
    return StartsWithNoCase(file, L"tree_08.");
}

bool IsModelFolder(std::wstring_view folder)
{
    static constexpr std::array<std::wstring_view, 4> kFolders{L"Item", L"Player", L"Monster", L"NPC"};
    for (const std::wstring_view known : kFolders)
    {
        if (EqualsNoCase(folder, known))
        {
            return true;
        }
    }
    // Map objects: Object1 ... Object99.
    constexpr std::wstring_view kObject = L"Object";
    if (folder.size() <= kObject.size() || !EqualsNoCase(folder.substr(0, kObject.size()), kObject))
    {
        return false;
    }
    return std::all_of(folder.begin() + static_cast<std::ptrdiff_t>(kObject.size()), folder.end(),
                       [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

// Box blur of `values` (width x height) with the given radius, clamped at the edges.
std::vector<float> BoxBlur(const std::vector<float>& values, int width, int height, int radius)
{
    if (radius <= 0)
    {
        return values;
    }
    std::vector<float> rows(values.size());
    for (int y = 0; y < height; ++y)
    {
        const float* in = values.data() + static_cast<std::size_t>(y) * width;
        float* out = rows.data() + static_cast<std::size_t>(y) * width;
        float sum = 0.0f;
        for (int k = -radius; k <= radius; ++k)
        {
            sum += in[std::clamp(k, 0, width - 1)];
        }
        for (int x = 0; x < width; ++x)
        {
            out[x] = sum;
            sum += in[std::min(x + radius + 1, width - 1)] - in[std::max(x - radius, 0)];
        }
    }
    std::vector<float> result(values.size());
    const float norm = 1.0f / static_cast<float>((2 * radius + 1) * (2 * radius + 1));
    for (int x = 0; x < width; ++x)
    {
        float sum = 0.0f;
        for (int k = -radius; k <= radius; ++k)
        {
            sum += rows[static_cast<std::size_t>(std::clamp(k, 0, height - 1)) * width + x];
        }
        for (int y = 0; y < height; ++y)
        {
            result[static_cast<std::size_t>(y) * width + x] = sum * norm;
            sum += rows[static_cast<std::size_t>(std::min(y + radius + 1, height - 1)) * width + x] -
                   rows[static_cast<std::size_t>(std::max(y - radius, 0)) * width + x];
        }
    }
    return result;
}

// Central difference of `h` at (x, y) over `step` texels, per texel.
void Gradient(const std::vector<float>& h, int width, int height, int x, int y, int step, float& gx, float& gy)
{
    const int x0 = std::max(x - step, 0);
    const int x1 = std::min(x + step, width - 1);
    const int y0 = std::max(y - step, 0);
    const int y1 = std::min(y + step, height - 1);
    const std::size_t row = static_cast<std::size_t>(y) * width;
    gx = x1 > x0 ? (h[row + x1] - h[row + x0]) / static_cast<float>(x1 - x0) : 0.0f;
    gy = y1 > y0 ? (h[static_cast<std::size_t>(y1) * width + x] - h[static_cast<std::size_t>(y0) * width + x]) /
                       static_cast<float>(y1 - y0)
                 : 0.0f;
}

std::uint8_t Encode(float tilt)
{
    return static_cast<std::uint8_t>(std::clamp(std::lround(128.0f + tilt * 127.0f), 1L, 255L));
}
} // namespace

void SetEnabled(bool enabled)
{
    s_enabled = enabled;
}

bool Enabled()
{
    return s_enabled;
}

void SetRuntimeBuild(bool enabled)
{
    s_runtimeBuild = enabled;
}

bool RuntimeBuild()
{
    return s_runtimeBuild;
}

bool WantsNormalMap(std::wstring_view path)
{
    // "Data" sep "HD" sep folder sep file
    std::array<std::wstring_view, 3> parts{};
    std::size_t start = 0;
    for (std::wstring_view& part : parts)
    {
        std::size_t end = start;
        while (end < path.size() && !IsSeparator(path[end]))
        {
            ++end;
        }
        if (end >= path.size())
        {
            return false;
        }
        part = path.substr(start, end - start);
        start = end + 1;
    }
    return EqualsNoCase(parts[0], L"Data") && EqualsNoCase(parts[1], L"HD") && IsModelFolder(parts[2]) &&
           start < path.size() && !(EqualsNoCase(parts[2], L"Player") && IsPlayerFace(FileName(path))) &&
           !IsLeafLitter(FileName(path));
}

std::vector<std::uint8_t> Build(std::span<const std::uint8_t> rgba, int width, int height, int scale)
{
    if (width <= 0 || height <= 0 ||
        rgba.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u)
    {
        return {};
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    // The height is the brightness of the visible texels only. A cut-out (a leaf on a clear background) must not
    // rise from nothing at its edge: that cliff turned small strewn leaves half black under the side light
    // (Lorencia fountain, 2026-11-11). So every blur is weighted by the alpha, and clear texels add nothing.
    std::vector<float> alpha(count);
    std::vector<float> weighted(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint8_t* p = rgba.data() + i * 4u;
        const float luminance = (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) / 255.0f;
        alpha[i] = p[3] / 255.0f;
        weighted[i] = luminance * alpha[i];
    }

    // Fine relief: lightly smoothed against JPEG noise. Broad relief: the shapes over `scale` texels.
    const int broadRadius = std::max(scale, 1) * 2;
    const std::vector<float> fineWeighted = BoxBlur(weighted, width, height, 1);
    const std::vector<float> fineAlpha = BoxBlur(alpha, width, height, 1);
    const std::vector<float> broadWeighted = BoxBlur(fineWeighted, width, height, broadRadius);
    const std::vector<float> broadAlpha = BoxBlur(fineAlpha, width, height, broadRadius);
    std::vector<float> fine(count);
    std::vector<float> broad(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        fine[i] = fineAlpha[i] > kMinCoverage ? fineWeighted[i] / fineAlpha[i] : 0.0f;
        broad[i] = broadAlpha[i] > kMinCoverage ? broadWeighted[i] / broadAlpha[i] : 0.0f;
    }

    std::vector<std::uint8_t> normals(count * 4u);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float fx = 0.0f;
            float fy = 0.0f;
            float bx = 0.0f;
            float by = 0.0f;
            Gradient(fine, width, height, x, y, 1, fx, fy);
            Gradient(broad, width, height, x, y, broadRadius, bx, by);
            const float broadScale = kBroadStrength * static_cast<float>(broadRadius);
            // The surface rises where the image brightens, so the normal leans the other way.
            float tx = -(fx * kFineStrength + bx * broadScale);
            float ty = -(fy * kFineStrength + by * broadScale);
            // From a slope to the tilt of a unit normal (x, y, 1) / length.
            const float inverseLength = 1.0f / std::sqrt(1.0f + tx * tx + ty * ty);
            tx *= inverseLength;
            ty *= inverseLength;
            const float tilt = std::sqrt(tx * tx + ty * ty);
            if (tilt > kMaxTilt)
            {
                tx *= kMaxTilt / tilt;
                ty *= kMaxTilt / tilt;
            }
            // A half-clear texel leans only as far as it shows; a clear one stays flat.
            const float shown = alpha[static_cast<std::size_t>(y) * width + x];
            tx *= shown;
            ty *= shown;
            std::uint8_t* out = normals.data() + (static_cast<std::size_t>(y) * width + x) * 4u;
            out[0] = Encode(tx);
            out[1] = Encode(ty);
            out[2] = 255;
            out[3] = 255;
        }
    }
    return normals;
}

} // namespace Render::Textures::NormalMaps
