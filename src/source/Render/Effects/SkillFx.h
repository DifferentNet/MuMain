// SkillFx.h: the Remaster skill effect looks (desktop, HD textures on), from the player's [Render]
// SkillLightPools, SoftParticles and FlipbookBlending settings and the effect level (Options >
// Graphics, [Render] EffectLevel 0..4).
//
//   LightPools     the lights of skill effects (fire, lightning, explosions) drawn on the ground as
//                  smooth round pools, beside the map's torch pools (Render/Terrain/TorchLights.h).
//   SoftParticles  effect sprites fade where they cut into the ground, walls and models, when the
//                  frame already has its world depth (SSAO or haze on; MuRendererSDLGpu).
//   FlipbookBlend  explosion and fire sheets blend from one frame to the next (FlipbookBlend.h).
//
//   Effect level 4: 16 pools, soft, flipbook; 3: 12 pools, soft, flipbook; 2: 8 pools, soft;
//   0 and 1: all off. In the Classic client (no Data/HD) and on the web all three are off and the
//   frame is the same as without them. Console: $fx pools|soft|flipbook on|off, $fx status.
//   Main thread only.
#pragma once

#include <cstdint>

namespace Render::Effects::SkillFx
{

enum class Feature : std::uint8_t
{
    LightPools,
    SoftParticles,
    FlipbookBlend,
};
inline constexpr int kFeatureCount = 3;

// The most ground pools of effect lights a frame draws (at effect level 4).
inline constexpr int kMaxPools = 16;

// The pools of effect lights at an effect level.
[[nodiscard]] constexpr int PoolBudget(const int level)
{
    if (level >= 4)
    {
        return kMaxPools;
    }
    if (level == 3)
    {
        return 12;
    }
    return level == 2 ? 8 : 0;
}

// Whether the effect level lets a feature show.
[[nodiscard]] constexpr bool LevelAllows(const Feature feature, const int level)
{
    switch (feature)
    {
    case Feature::LightPools:
        return PoolBudget(level) > 0;
    case Feature::SoftParticles:
        return level >= 2;
    case Feature::FlipbookBlend:
        return level >= 3;
    }
    return false;
}

// A feature is on when the player chose it, the client offers it and the effect level lets it show.
[[nodiscard]] constexpr bool Resolve(const bool chosen, const bool available, const Feature feature,
                                     const int level)
{
    return chosen && available && LevelAllows(feature, level);
}

// The console name of a feature: pools, soft, flipbook.
[[nodiscard]] constexpr const char* Name(const Feature feature)
{
    switch (feature)
    {
    case Feature::LightPools:
        return "pools";
    case Feature::SoftParticles:
        return "soft";
    case Feature::FlipbookBlend:
        return "flipbook";
    }
    return "";
}

// Whether this client offers the looks at all: the Remaster PC client (never the web, never Classic).
[[nodiscard]] bool Available();
// Reads the three settings from GameConfig (startup, graphics preset changes).
void ApplyFromConfig();
// Whether a feature shows now (cheap: called for each effect sprite).
[[nodiscard]] bool Enabled(Feature feature);
// The pools of effect lights this frame: PoolBudget(effect level), 0 when LightPools is off.
[[nodiscard]] int ActivePoolBudget();
// Whether the player chose a feature ([Render] setting), whatever the level and the client.
[[nodiscard]] bool Chosen(Feature feature);
// Saves a feature's setting ($fx) and applies it at once.
void Choose(Feature feature, bool enabled);

} // namespace Render::Effects::SkillFx
