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

// Intel graphics other than the Arc cards; AMD's integrated Radeon: no "RX", "Pro" or "FirePro" card model,
// and named "... Graphics" ("Radeon(TM) Graphics", "Radeon 780M Graphics", the Ryzen APUs' "RX Vega 11
// Graphics"), a mobile model number ending in "M" ("Radeon(TM) 610M") or "Vega".
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
    const bool vegaGraphics =
        lowerName.find("vega") != std::string::npos && lowerName.find("graphics") != std::string::npos;
    if (vegaGraphics)
    {
        return true;
    }
    if (ContainsAny(lowerName, {"rx", "pro", "fire"}))
    {
        return false;
    }
    if (ContainsAny(lowerName, {"graphics", "vega"}))
    {
        return true;
    }
    // A model number ending in "m": "610m", "680m".
    for (std::size_t i = 1; i < lowerName.size(); ++i)
    {
        const bool atWordEnd = i + 1 == lowerName.size() || lowerName[i + 1] == ' ';
        if (lowerName[i] == 'm' && atWordEnd && lowerName[i - 1] >= '0' && lowerName[i - 1] <= '9')
        {
            return true;
        }
    }
    return false;
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
