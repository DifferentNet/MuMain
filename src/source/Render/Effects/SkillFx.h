// SkillFx.h: the Remaster skill effect looks (desktop, HD textures on), from the player's [Render]
// SkillLightPools, SoftParticles, FlipbookBlending, SmoothTrails, SkillGroundMarks and SkillHotCores settings and
// the effect level (Options > Graphics, [Render] EffectLevel 0..4).
//
//   LightPools     the lights of skill effects (fire, lightning, explosions) drawn on the ground as
//                  smooth round pools, beside the map's torch pools (Render/Terrain/TorchLights.h).
//   SoftParticles  effect sprites fade where they cut into the ground, walls and models, when the
//                  frame already has its world depth (SSAO or haze on; MuRendererSDLGpu).
//   FlipbookBlend  explosion and fire sheets blend from one frame to the next (FlipbookBlend.h).
//   Ribbons        the Evil Spirit and Death Stab trails are drawn as smooth curves whose texture and
//                  end move every frame (RibbonSpline.h, JointRibbon.h), and the sword swing trail no
//                  longer folds over itself at high frame rates (BlurSlice.h).
//   Decals         fire, ice and earth skills leave a fading mark on the ground (scorch, frost;
//                  GroundMarks.h).
//   Cores          where glowing (additive) effects pile up bright, the frame's centre of the pile burns
//                  toward white in the effects' own colour, in the post-processing composite
//                  (PostProcessing/HotCores.h, post_composite_cores.frag). Needs post-processing and bloom.
//
//   Effect level 4: 16 pools, soft, flipbook, trails of up to 8 pieces a span, 48 ground marks, cores at
//   full strength; 3: 12 pools, soft, flipbook, trails of up to 4 pieces, 32 marks, cores 0.75; 2: 8 pools,
//   soft, 16 marks, cores 0.5; 0 and 1: all off. In the Classic client (no Data/HD) and on the web all six
//   are off and the frame is the same as without them.
//   Console: $fx pools|soft|flipbook|trails|decals|cores on|off, $fx status.
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
    Ribbons,
    Decals,
    Cores,
};
inline constexpr int kFeatureCount = 6;

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

// The most pieces a smooth trail cuts one span between two tails into, at an effect level (0: classic).
[[nodiscard]] constexpr int RibbonPieceBudget(const int level)
{
    if (level >= 4)
    {
        return 8;
    }
    return level == 3 ? 4 : 0;
}

// The most smooth trail quads a frame draws at an effect level; past it trails draw as classic.
[[nodiscard]] constexpr int RibbonQuadBudget(const int level)
{
    if (level >= 4)
    {
        return 4096;
    }
    return level == 3 ? 2048 : 0;
}

// The most ground marks (GroundMarks.h) at once (at effect level 4).
inline constexpr int kMaxMarks = 48;

// The ground marks that may lie at once at an effect level (0: none).
[[nodiscard]] constexpr int MarkBudget(const int level)
{
    if (level >= 4)
    {
        return kMaxMarks;
    }
    if (level == 3)
    {
        return 32;
    }
    return level == 2 ? 16 : 0;
}

// The strength of the hot cores at an effect level (0: off; HotCores.h scales the whitening by it).
[[nodiscard]] constexpr float CoreStrength(const int level)
{
    if (level >= 4)
    {
        return 1.0f;
    }
    if (level == 3)
    {
        return 0.75f;
    }
    return level == 2 ? 0.5f : 0.0f;
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
    case Feature::Ribbons:
        return RibbonPieceBudget(level) > 0;
    case Feature::Decals:
        return MarkBudget(level) > 0;
    case Feature::Cores:
        return CoreStrength(level) > 0.0f;
    }
    return false;
}

// A feature is on when the player chose it, the client offers it and the effect level lets it show.
[[nodiscard]] constexpr bool Resolve(const bool chosen, const bool available, const Feature feature,
                                     const int level)
{
    return chosen && available && LevelAllows(feature, level);
}

// The console name of a feature: pools, soft, flipbook, trails, decals, cores.
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
    case Feature::Ribbons:
        return "trails";
    case Feature::Decals:
        return "decals";
    case Feature::Cores:
        return "cores";
    }
    return "";
}

// Whether this client offers the looks at all: the Remaster PC client (never the web, never Classic).
[[nodiscard]] bool Available();
// Reads the six settings from GameConfig (startup, graphics preset changes).
void ApplyFromConfig();
namespace Detail
{
// Whether this client offers the looks (Available(), kept by ApplyFromConfig); read by Enabled.
extern bool g_available;
// Enabled when the client offers the looks.
[[nodiscard]] bool EnabledWhenAvailable(Feature feature);
} // namespace Detail

// Whether a feature shows now (cheap: called for each effect sprite). The web and the Classic client
// never offer the looks, so they answer here without a call.
[[nodiscard]] inline bool Enabled(const Feature feature)
{
#ifdef __EMSCRIPTEN__
    static_cast<void>(feature);
    return false;
#else
    return Detail::g_available && Detail::EnabledWhenAvailable(feature);
#endif
}
// The pools of effect lights this frame: PoolBudget(effect level), 0 when LightPools is off.
[[nodiscard]] int ActivePoolBudget();
// The pieces a smooth trail span may take this frame: RibbonPieceBudget(effect level), 0 when Ribbons is off.
[[nodiscard]] int ActiveRibbonPieces();
// The smooth trail quads this frame may draw: RibbonQuadBudget(effect level), 0 when Ribbons is off.
[[nodiscard]] int ActiveRibbonQuadBudget();
// The ground marks that may lie at once now: MarkBudget(effect level), 0 when Decals is off.
[[nodiscard]] int ActiveMarkBudget();
// The hot core strength this frame: CoreStrength(effect level), 0 when Cores is off (the composite then runs
// unchanged).
[[nodiscard]] float ActiveCoreStrength();
// Whether the player chose a feature ([Render] setting), whatever the level and the client.
[[nodiscard]] bool Chosen(Feature feature);
// Saves a feature's setting ($fx) and applies it at once.
void Choose(Feature feature, bool enabled);

} // namespace Render::Effects::SkillFx
