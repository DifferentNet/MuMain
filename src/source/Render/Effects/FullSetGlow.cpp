#include "stdafx.h"
#include "Render/Effects/FullSetGlow.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzObject.h"
#include "GameLogic/Items/AncientAura.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <algorithm>
#include <cmath>

namespace Render::Effects
{
namespace
{
// The gold of the +14/+15 ancient tier, as light factors.
constexpr float Gold[3] = {1.0f, 0.72f, 0.18f};

// The lights at the hands are drawn at half the armour's brightness (PartObjectColor's Bright).
constexpr float HandBrightness = 0.5f;

// The light falling from the boots, a little darker than the hands' gold, as the sets' own colours are.
constexpr float BootsGoldShare = 0.75f;

// The +15 eyes: the bone of the head and the lights' offsets from it (left eye; the right one mirrors z).
constexpr int HeadBone = 20;
constexpr float EyeOffset[3] = {13.0f, 10.0f, 3.0f};
constexpr int LeftEyeTrail = 55;
constexpr int RightEyeTrail = 56;
constexpr float TrailScale = 6.0f;
constexpr float EyePulseSpeed = 0.002f;
constexpr float ShineScale = 0.5f;
constexpr float PinLightScale = 1.3f;
constexpr float PinLightBase = 0.5f;
constexpr float LeftPinRotation = 100.0f;
constexpr float RightPinRotation = 80.0f;

// The eyes' colours: the trail, the shine and the pin light; the game's blue ones and the gold ones.
struct EyeColors
{
    float Trail[3];
    float Shine[3];
    float Pin[3];
};

constexpr EyeColors BlueEyes = {{0.09f, 0.09f, 0.8f}, {0.2f, 0.4f, 0.8f}, {0.1f, 0.15f, 1.0f}};
constexpr EyeColors GoldEyes = {{0.8f, 0.55f, 0.09f}, {0.8f, 0.6f, 0.2f}, {1.0f, 0.7f, 0.1f}};

void Scaled(const float* color, float factor, vec3_t light)
{
    Vector(color[0] * factor, color[1] * factor, color[2] * factor, light);
}

void RenderEye(OBJECT* o, BMD* b, bool left, const EyeColors& colors, bool spawnTrail, float pulse)
{
    vec3_t offset;
    vec3_t position;
    vec3_t color;
    Vector(EyeOffset[0], EyeOffset[1], left ? EyeOffset[2] : -EyeOffset[2], offset);
    b->TransformPosition(o->BoneTransform[HeadBone], offset, position, true);
    VectorCopy(position, left ? o->EyeLeft : o->EyeRight);

    Scaled(colors.Trail, 1.0f, color);
    // The sprites draw every frame; the trail joint spawns at the reference rate.
    if (spawnTrail)
        CreateJoint(BITMAP_JOINT_ENERGY, position, o->Position, o->Angle, left ? LeftEyeTrail : RightEyeTrail, o,
                    TrailScale, -1, 0, 0, -1, color);

    Scaled(colors.Shine, 1.0f, color);
    CreateSprite(BITMAP_SHINY + 6, position, ShineScale * pulse, color, o);

    Scaled(colors.Pin, 1.0f, color);
    CreateSprite(BITMAP_PIN_LIGHT, position, PinLightScale * pulse + PinLightBase, color, o,
                 left ? LeftPinRotation : RightPinRotation);
}
} // namespace

FullSetGlow FullSetGlowOf(const CHARACTER* c, bool sameKindSet, int sameKindLevel)
{
    FullSetGlow glow;
    glow.Draw = sameKindSet;
    glow.Level = sameKindSet ? sameKindLevel : 0;

    const int tier = GameLogic::AncientAura::TierOf(c);
    if (tier == GameLogic::AncientAura::None)
        return glow;

    glow.Draw = true;
    glow.Gold = true;
    glow.Level = std::max(glow.Level, tier);
    return glow;
}

void FullSetHandLight(const CHARACTER* c, const OBJECT* o, const FullSetGlow& glow, vec3_t light)
{
    if (!glow.Gold)
    {
        PartObjectColor(c->BodyPart[BODYPART_BOOTS].Type, o->Alpha, HandBrightness, light);
        return;
    }

    Scaled(Gold, HandBrightness * o->Alpha, light);
}

void FullSetFlareLight(const FullSetGlow& glow, vec3_t light)
{
    if (glow.Gold)
        Scaled(Gold, 1.0f, light);
    else
        Vector(1.f, 1.f, 1.f, light);
}

void FullSetBootsLight(const FullSetGlow& glow, vec3_t light)
{
    if (glow.Gold)
        Scaled(Gold, BootsGoldShare, light);
}

void RenderFullSetEyes(OBJECT* o, BMD* b, bool gold)
{
    const bool spawnTrails = rand_fps_check(1);
    const float pulse = sinf(static_cast<float>(WorldTime * EyePulseSpeed));
    const EyeColors& colors = gold ? GoldEyes : BlueEyes;
    RenderEye(o, b, true, colors, spawnTrails, pulse);
    RenderEye(o, b, false, colors, spawnTrails, pulse);
}
} // namespace Render::Effects
