#include "Render/Look/GraphicsPreset.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace Render::Look
{
namespace
{
struct PresetRow
{
    GraphicsPreset preset;
    GraphicsSettings settings;
};

// look, lighting, dynamic lights, light mood, post-processing, anti-aliasing, FXAA, sharpen, anisotropy, sun shadows.
constexpr std::array<PresetRow, 4> PresetRows{{
    {GraphicsPreset::Low, {LookPreset::Classic, false, false, false, false, 0, false, 0, 1, 0}},
    {GraphicsPreset::Medium, {LookPreset::Enhanced, true, false, true, true, 0, true, 30, 4, 0}},
    {GraphicsPreset::High, {LookPreset::Enhanced, true, true, true, true, 4, false, 30, 8, 1}},
    {GraphicsPreset::Ultra, {LookPreset::Enhanced, true, true, true, true, 8, false, 30, 16, 2}},
}};

std::string Lowercase(std::string_view text)
{
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

bool ContainsAny(const std::string& lowerName, std::initializer_list<std::string_view> needles)
{
    return std::any_of(needles.begin(), needles.end(),
                       [&lowerName](std::string_view needle) { return lowerName.find(needle) != std::string::npos; });
}

bool IsSoftwareRenderer(const std::string& lowerName)
{
    return ContainsAny(lowerName, {"microsoft basic render", "llvmpipe", "swiftshader", "warp"});
}

// Intel graphics other than the Arc cards; AMD's integrated Radeon (named "Radeon(TM) Graphics",
// "Radeon Graphics" or "Vega", without an "RX" card model).
bool IsIntegratedGraphics(const std::string& lowerName)
{
    if (lowerName.find("intel") != std::string::npos)
    {
        return lowerName.find("arc") == std::string::npos;
    }
    if (lowerName.find("amd") == std::string::npos && lowerName.find("radeon") == std::string::npos)
    {
        return false;
    }
    if (lowerName.find("rx") != std::string::npos)
    {
        return false;
    }
    return ContainsAny(lowerName, {"radeon(tm) graphics", "radeon graphics", "vega"});
}
} // namespace

GraphicsSettings GraphicsPresetSettings(GraphicsPreset preset)
{
    for (const PresetRow& row : PresetRows)
    {
        if (row.preset == preset)
        {
            return row.settings;
        }
    }
    return PresetRows.front().settings;
}

GraphicsPreset MatchGraphicsPreset(const GraphicsSettings& settings)
{
    for (const PresetRow& row : PresetRows)
    {
        if (row.settings == settings)
        {
            return row.preset;
        }
    }
    return GraphicsPreset::Custom;
}

GraphicsPreset ChooseAutoGraphicsPreset(std::string_view gpuName)
{
    const std::string lowerName = Lowercase(gpuName);
    if (IsSoftwareRenderer(lowerName))
    {
        return GraphicsPreset::Low;
    }
    if (lowerName.empty() || lowerName.find("unknown") != std::string::npos)
    {
        return GraphicsPreset::Medium;
    }
    if (IsIntegratedGraphics(lowerName))
    {
        return GraphicsPreset::Medium;
    }
    return GraphicsPreset::High;
}

} // namespace Render::Look
