#include "stdafx.h"
#include "Render/Effects/SeasonAura.h"

#include "GameLogic/Events/SeasonMarks.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Render::Effects
{
namespace
{
// The graphics level from which the full set's glow is drawn (RenderCharacter).
constexpr int MinimumRenderLevel = 2;

// The circle's two layers, in terrain tiles across: the runes turning one way and a faint wider ring the other way.
constexpr float InnerCircle = 1.3f;
constexpr float OuterCircle = 1.9f;

// About one flake for every this many frames of 25 per second, each living two to three seconds: some ten flakes
// around a character at a time.
constexpr int FlakeEveryFrames = 6;

// Up to this many auras in sight snow fully; with more, each snows less, so that all of them together keep to some
// 120 flakes of the 3,000 particles every effect shares (MAX_PARTICLES): a crowded town loses no skill effects.
constexpr int FullySnowingAuras = 12;

// The auras drawn in the last frame and in this one (WorldTime changes once a frame).
int g_aurasLastFrame = 0;
int g_aurasThisFrame = 0;
double g_auraFrame = -1.0;

void CountAura()
{
    if (WorldTime != g_auraFrame)
    {
        g_auraFrame = WorldTime;
        g_aurasLastFrame = g_aurasThisFrame;
        g_aurasThisFrame = 0;
    }
    ++g_aurasThisFrame;
}

void RenderCircle(OBJECT* o, const GameLogic::SeasonMarks::SeasonLook& look)
{
    if (look.CircleTexture != GameLogic::SeasonMarks::Circle::Runes)
        return;

    const float time = static_cast<float>(WorldTime);
    const float pulse = sinf(time * 0.002f) * 0.15f + 0.75f;
    vec3_t light;
    Vector(look.CircleColor[0] * pulse, look.CircleColor[1] * pulse, look.CircleColor[2] * pulse, light);

    EnableAlphaBlend();
    RenderTerrainAlphaBitmap(BITMAP_MAGIC_ZIN, o->Position[0], o->Position[1], InnerCircle, InnerCircle, light,
                             time * 0.004f);

    Vector(light[0] * 0.45f, light[1] * 0.45f, light[2] * 0.45f, light);
    RenderTerrainAlphaBitmap(BITMAP_MAGIC_ZIN, o->Position[0], o->Position[1], OuterCircle, OuterCircle, light,
                             -time * 0.0025f);
}

void CreateParticles(OBJECT* o, const GameLogic::SeasonMarks::SeasonLook& look)
{
    if (look.ParticleKind != GameLogic::SeasonMarks::Particles::Snow)
        return;

    // Shared evenly: 13 to 24 auras snow half as often, 25 to 36 a third, and so on.
    const int share = (std::max(g_aurasLastFrame, 1) + FullySnowingAuras - 1) / FullySnowingAuras;
    if (!rand_fps_check(FlakeEveryFrames * share))
        return;

    // A flake starts somewhere over the circle, above the head, and falls (SubType 1 in ZzzEffectParticle.cpp).
    const float angle = static_cast<float>(rand() % 360) * Q_PI / 180.f;
    const float radius = static_cast<float>(rand() % 50 + 20);
    vec3_t position;
    Vector(o->Position[0] + cosf(angle) * radius, o->Position[1] + sinf(angle) * radius,
           o->Position[2] + static_cast<float>(rand() % 60 + 170), position);

    vec3_t light;
    Vector(look.ParticleColor[0], look.ParticleColor[1], look.ParticleColor[2], light);
    CreateParticle(rand() % 2 == 0 ? BITMAP_SNOW_EFFECT_1 : BITMAP_SNOW_EFFECT_2, position, o->Angle, light, 1, 0.3f);
}
} // namespace

void RenderSeasonAura(CHARACTER* c, OBJECT* o)
{
    if (c == nullptr || o == nullptr || SceneFlag != MAIN_SCENE || o->Kind != KIND_PLAYER || !o->Visible)
        return;

    // Not where everyone looks alike, not on someone unseen, and only with the full set's glow.
    if (gMapManager.InChaosCastle() || g_isCharacterBuff(o, eBuff_Cloaking) ||
        g_pOption->GetRenderLevel() < MinimumRenderLevel)
        return;

    auto& marks = GameLogic::SeasonMarks::Marks();
    if (marks.Count() == 0)
        return;

    const auto worn = marks.Of(c->ID);
    if (worn.AuraSeason < 0)
        return;

    const auto& look = marks.Look(worn.AuraSeason);
    CountAura();
    RenderCircle(o, look);
    CreateParticles(o, look);
}
} // namespace Render::Effects
