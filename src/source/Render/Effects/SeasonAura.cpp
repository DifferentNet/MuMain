#include "stdafx.h"
#include "Render/Effects/SeasonAura.h"

#include "GameLogic/Events/SeasonMarks.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "World/MapInfra/MapManager.h"

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
    if (look.ParticleKind != GameLogic::SeasonMarks::Particles::Snow || !rand_fps_check(FlakeEveryFrames))
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
    RenderCircle(o, look);
    CreateParticles(o, look);
}
} // namespace Render::Effects
