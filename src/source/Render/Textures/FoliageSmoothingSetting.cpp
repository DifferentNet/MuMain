#include "stdafx.h"

#include "Render/Textures/FoliageSmoothing.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Render/Textures/HdTextures.h"
#include "Render/Textures/TextureFiltering.h"

#include <cmath>
#include <optional>

namespace Render::Textures::FoliageSmoothing
{

bool Available()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    return true;
#endif
}

namespace
{
[[nodiscard]] bool LowPresetChosen()
{
    return GameConfig::GetInstance().GetGraphicsPreset() == GraphicsPreset::Low;
}

float s_strength = 0.0f;
// The strength last given, for logging only its changes.
std::optional<float> s_appliedStrength;
} // namespace

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    const bool hdTextures = Render::Textures::Hd::Enabled();
    s_strength = Resolve(config.GetFoliageSmoothing(), config.GetFoliageSmoothingStrength(),
                         Available() && hdTextures, LowPresetChosen());
    // The textures switch their cut-out samplers between frames, and only when the bias changes.
    Bitmaps.SetCutoutMipBias(CutoutMipLodBias(MipLodBias(hdTextures), s_strength));
    if (s_appliedStrength == s_strength)
    {
        return;
    }
    s_appliedStrength = s_strength;
    mu::log::Get("render")->info("Foliage smoothing {} (strength {} %)", s_strength > 0.0f ? "on" : "off",
                                 static_cast<int>(std::lround(s_strength * 100.0f)));
}

float Strength()
{
    return s_strength;
}

Block CurrentBlock()
{
    return ResolveBlock(Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
}

void Choose(bool enabled, int strengthPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetFoliageSmoothing(enabled ? CfgDefaults::CfgMaxFoliageSmoothing : CfgDefaults::CfgMinFoliageSmoothing);
    config.SetFoliageSmoothingStrength(strengthPercent);
    config.Save();
    ApplyFromConfig();
}

} // namespace Render::Textures::FoliageSmoothing
