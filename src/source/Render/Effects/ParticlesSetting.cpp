#include "stdafx.h"

#include "Render/Effects/ParticlesSetting.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Lighting/LightMoodSetting.h"
#include "Render/PostProcessing/HazeSetting.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/SdlGpuParticles.h"
#include "Render/Textures/HdTextures.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/Core/WindowSystem.h"
#include "World/MapInfra/MapManager.h"

#include <optional>

namespace Render::Effects::Particles
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
constexpr double kMillisecondsPerSecond = 1000.0;
// Below this presence the layers are not drawn at all.
constexpr float kHiddenPresence = 0.01f;
// The layers' hash seeds (plus the kind's number), so two layers never share their places.
constexpr float kFirstLayerSeed = 17.0f;
constexpr float kSecondLayerSeed = 101.0f;

[[nodiscard]] bool LowPresetChosen()
{
    return GameConfig::GetInstance().GetGraphicsPreset() == GraphicsPreset::Low;
}

float s_strength = 0.0f;
// The strength last given to the renderer, for logging only its changes.
std::optional<float> s_appliedStrength;

// The hero's smoothed ground height and the presence, the map and the time they were last worked
// out for.
struct Follow
{
    int world = -1;
    float height = 0.0f;
    float presence = 0.0f;
    double time = 0.0;
};
Follow s_follow;

struct LayerSource
{
    Kind kind = Kind::None;
    float density = 1.0f;
    float seed = 0.0f;
};

void FillLayer(const LayerSource& source, int count, const float tint[3], float referenceHeight, float presence,
               float phase, mu::AmbientParticleLayer& layer)
{
    const Look& look = LookOf(source.kind);
    const OBJECT& hero = Hero->Object;
    layer.count = static_cast<std::uint32_t>(count);
    layer.boxCenter[0] = hero.Position[0];
    layer.boxCenter[1] = hero.Position[1];
    layer.boxCenter[2] = referenceHeight + look.zMin + look.zSize * 0.5f;
    layer.boxSize[0] = kBoxSizeXY;
    layer.boxSize[1] = kBoxSizeXY;
    layer.boxSize[2] = look.zSize;
    layer.seed = source.seed + static_cast<float>(source.kind);
    const float drift = Turns(look.drift * WindShare(Render::Models::Wind::Strength()), kBoxSizeXY);
    layer.travel[0] = Render::Models::Wind::kDirectionX * drift;
    layer.travel[1] = Render::Models::Wind::kDirectionY * drift;
    layer.travel[2] = Turns(look.fall, look.zSize);
    layer.travel[3] = look.speedSpread;
    const auto period = static_cast<float>(kPeriodSeconds);
    layer.sway[0] = look.swayXY;
    layer.sway[1] = look.swayZ;
    layer.sway[2] = look.swayHz * period;
    layer.sway[3] = look.flickerHz * period;
    layer.radius[0] = look.minRadius;
    layer.radius[1] = look.maxRadius;
    layer.pixels[0] = look.minPixels;
    layer.pixels[1] = look.maxPixels;
    // A dim mood's background darkens the soft kinds, so dust never glows over a dark map; the
    // glowing kinds keep their light.
    const float background = look.additive ? 1.0f : Render::Lighting::LightMood::Active().background;
    for (int channel = 0; channel < 3; ++channel)
    {
        layer.color[channel] = look.color[channel] * tint[channel] * background;
    }
    layer.color[3] = look.alpha * presence;
    layer.look[0] = look.additive ? 1.0f : 0.0f;
    layer.look[1] = look.glow ? 1.0f : 0.0f;
    layer.look[2] = look.flickerDepth;
    layer.look[3] = look.riseFade;
    layer.phase = phase;
}
} // namespace

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    s_strength = Resolve(config.GetParticles(), config.GetParticlesStrength(),
                         Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
    mu::SetParticlesSettings(s_strength);
    if (s_appliedStrength == s_strength)
    {
        return;
    }
    s_appliedStrength = s_strength;
    mu::log::Get("render")->info("Particles {} (density {} %)", s_strength > 0.0f ? "on" : "off",
                                 static_cast<int>(std::lround(s_strength * kDefaultPercent)));
}

Block CurrentBlock()
{
    return ResolveBlock(Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
}

void Choose(bool enabled, int strengthPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetParticles(enabled ? CfgDefaults::CfgMaxParticles : CfgDefaults::CfgMinParticles);
    config.SetParticlesStrength(strengthPercent);
    config.Save();
    ApplyFromConfig();
}

void PushFrame()
{
    if (s_strength <= 0.0f || !Hero || !g_pOption->GetRenderAllEffects())
    {
        return;
    }
    const Data::Maps::LightMood& mood = Render::Lighting::LightMood::WorldMood();
    const std::array<LayerSource, kMaxLayers> sources{{
        {KindFromValue(mood.particles), static_cast<float>(mood.particlesDensity), kFirstLayerSeed},
        {KindFromValue(mood.particles2), static_cast<float>(mood.particles2Density), kSecondLayerSeed},
    }};
    if (sources[0].kind == Kind::None && sources[1].kind == Kind::None)
    {
        return; // nothing recorded: the frame is as without particles
    }

    // The presence and the reference height follow the hero; both snap on a new map.
    const bool newMap = s_follow.world != gMapManager.WorldActive;
    const float seconds = static_cast<float>((WorldTime - s_follow.time) / kMillisecondsPerSecond);
    const float target = Outdoors(gMapManager.WorldActive, HeroTile) ? 1.0f : 0.0f;
    s_follow.presence = SmoothPresence(s_follow.presence, target, seconds, newMap);
    s_follow.height =
        Render::PostProcessing::Haze::SmoothReference(s_follow.height, Hero->Object.Position[2], seconds, newMap);
    s_follow.world = gMapManager.WorldActive;
    s_follow.time = WorldTime;
    if (s_follow.presence <= kHiddenPresence)
    {
        return;
    }

    const float tint[3] = {static_cast<float>(mood.particlesR), static_cast<float>(mood.particlesG),
                           static_cast<float>(mood.particlesB)};
    const float phase = Phase(WorldTime);
    mu::AmbientParticlesFrame frame{};
    for (const LayerSource& source : sources)
    {
        const int count = Count(source.kind, source.density, s_strength);
        if (count <= 0)
        {
            continue;
        }
        FillLayer(source, count, tint, s_follow.height, s_follow.presence, phase, frame.layers[frame.layerCount]);
        ++frame.layerCount;
    }
    if (frame.layerCount > 0)
    {
        mu::GetRenderer().DrawAmbientParticles(frame);
    }
}

} // namespace Render::Effects::Particles
