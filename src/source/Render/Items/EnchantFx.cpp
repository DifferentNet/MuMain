#include "stdafx.h"
#include "Render/Items/EnchantFx.h"

#include "Audio/DSPlaySound.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzObject.h"
#include "GameLogic/Items/AncientLook.h"
#include "GameLogic/Items/EnchantFlashLine.h"
#include "GameLogic/Items/ExactLevelLine.h"
#include "Network/Server/HiddenChatLines.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Items/EnchantFxTuning.h"
#include "Render/Items/ItemGlow.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Textures/HdTextures.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace Render::Items::EnchantFx
{
namespace
{
namespace T = Tuning;

// The pieces of a player that glow, with the bones of the player model their aura and sparks start from (the
// bones the Classic +15 armour objects hang on, NextGradeObjectRender). Weapons use their hand's bone.
struct ArmourPiece
{
    int bodyPart;
    int bones[2];
};

constexpr ArmourPiece ArmourPieces[] = {
    {BODYPART_HELM, {20, -1}},
    {BODYPART_ARMOR, {35, 26}},
    {BODYPART_PANTS, {3, 10}},
    {BODYPART_GLOVES, {36, 27}},
    {BODYPART_BOOTS, {4, 11}},
};

constexpr int HeadBone = 20;
constexpr int LeftLegBone = 3;
constexpr int RightLegBone = 10;
constexpr float MaxGlowingPieces = 7.0f;

// One glowing piece found this frame.
struct Piece
{
    int level = 0;
    vec3_t color{};
    vec3_t positions[2]{};
    int positionCount = 0;
};

// ---- Frame budgets (WorldTime changes once a frame) ----

double g_frame = -1.0;
float g_sparkDemandLastFrame = 0.0f;
float g_sparkDemandThisFrame = 0.0f;
int g_auraSpritesThisFrame = 0;

void StartFrame()
{
    if (WorldTime == g_frame)
        return;
    g_frame = WorldTime;
    g_sparkDemandLastFrame = g_sparkDemandThisFrame;
    g_sparkDemandThisFrame = 0.0f;
    g_auraSpritesThisFrame = 0;
}

bool TakeAuraSprite()
{
    if (g_auraSpritesThisFrame >= T::MaxAuraSpritesPerFrame)
        return false;
    ++g_auraSpritesThisFrame;
    return true;
}

// How much of its wish a spark emitter gets: everyone the same share when all together want more than the limit.
float SparkShare()
{
    if (g_sparkDemandLastFrame <= T::MaxSparksPerSecond)
        return 1.0f;
    return T::MaxSparksPerSecond / g_sparkDemandLastFrame;
}

float FrameSeconds()
{
    return static_cast<float>(FPS_ANIMATION_FACTOR / REFERENCE_FPS);
}

float Random01()
{
    return static_cast<float>(rand() % 1000) / 1000.0f;
}

// The level a piece glows like, as RenderPartObjectEffect draws it (model overrides, the graphics level's cap).
int GlowLevel(int modelType, int itemLevel)
{
    int level = Render::Items::Glow::GetLevel(modelType, itemLevel);
    const int renderLevel = g_pOption->GetRenderLevel();
    if (renderLevel < 4)
        level = std::min(level, renderLevel * 2 + 5);
    return level;
}

void GlowColor(int modelType, vec3_t color)
{
    PartObjectColor(modelType, 1.0f, 1.0f, color);
}

// A worn piece's level: the look packet gives +14 as 13, the server's #UPL line the exact one
// (GameLogic/Items/ExactLevelLine.h; piece 0-1 the hands, 2-6 helm to boots).
int PieceLevel(CHARACTER* c, int piece, int shownLevel)
{
    const int exact = GameLogic::AncientLook::ExactLevelOf(std::wstring_view(c->ID), piece);
    return GameLogic::Items::ExactLevelLine::Resolve(shownLevel, exact);
}

int CollectPieces(CHARACTER* c, OBJECT* o, BMD* b, std::array<Piece, 7>& pieces)
{
    int count = 0;
    vec3_t offset{};
    for (int i = 0; i < 2; ++i)
    {
        const PART_t& weapon = c->Weapon[i];
        if (weapon.Type == -1 || weapon.Type == MODEL_ARROWS || weapon.Type == MODEL_BOLT)
            continue;
        const int level = GlowLevel(weapon.Type, PieceLevel(c, i, weapon.Level));
        if (level < T::AuraMinLevel || level > 15)
            continue;
        Piece& piece = pieces[count++];
        piece.level = level;
        GlowColor(weapon.Type, piece.color);
        b->TransformPosition(o->BoneTransform[weapon.LinkBone], offset, piece.positions[0], true);
        piece.positionCount = 1;
    }
    for (const ArmourPiece& armour : ArmourPieces)
    {
        const PART_t& part = c->BodyPart[armour.bodyPart];
        if (part.Type == -1)
            continue;
        const int level = GlowLevel(part.Type, PieceLevel(c, GameLogic::Items::ExactLevelLine::FirstArmourPiece + armour.bodyPart - BODYPART_HELM, part.Level));
        if (level < T::AuraMinLevel || level > 15)
            continue;
        Piece& piece = pieces[count++];
        piece.level = level;
        GlowColor(part.Type, piece.color);
        piece.positionCount = 0;
        for (const int bone : armour.bones)
        {
            if (bone < 0)
                continue;
            b->TransformByObjectBone(piece.positions[piece.positionCount++], o, bone);
        }
    }
    return count;
}

void RenderAura(CHARACTER* c, OBJECT* o, BMD* b, const std::array<Piece, 7>& pieces, int count)
{
    const float wave = sinf(static_cast<float>(WorldTime) * T::AuraPulseSpeed + static_cast<float>(c->Key) * 0.7f);
    const float pulse = T::AuraPulseLow + (T::AuraPulseHigh - T::AuraPulseLow) * (0.5f + 0.5f * wave);

    vec3_t average{};
    for (int i = 0; i < count; ++i)
    {
        const Piece& piece = pieces[i];
        VectorAdd(average, piece.color, average);
        vec3_t light;
        VectorScale(piece.color, pulse, light);
        const float scale = T::AuraPieceScale[piece.level >= T::SparkMinLevel ? 1 : 0];
        for (int k = 0; k < piece.positionCount; ++k)
        {
            if (!TakeAuraSprite())
                return;
            vec3_t position;
            VectorCopy(piece.positions[k], position);
            CreateSprite(BITMAP_LIGHT, position, scale, light, o);
        }
    }
    VectorScale(average, 1.0f / static_cast<float>(count), average);

    // The halo around the whole body, bigger the more pieces glow.
    if (TakeAuraSprite())
    {
        vec3_t head, left, right, center;
        b->TransformByObjectBone(head, o, HeadBone);
        b->TransformByObjectBone(left, o, LeftLegBone);
        b->TransformByObjectBone(right, o, RightLegBone);
        for (int axis = 0; axis < 3; ++axis)
            center[axis] = (head[axis] + left[axis] + right[axis]) / 3.0f;
        const float share = std::min(1.0f, static_cast<float>(count) / MaxGlowingPieces);
        const float scale = T::AuraBodyScaleMin + (T::AuraBodyScaleMax - T::AuraBodyScaleMin) * share;
        vec3_t light;
        VectorScale(average, pulse * T::AuraBodyBrightness, light);
        CreateSprite(BITMAP_LIGHT, center, scale, light, o);
    }

    vec3_t ground;
    VectorScale(average, pulse * T::AuraGroundLight, ground);
    AddTerrainLight(o->Position[0], o->Position[1], ground, T::AuraGroundRange, PrimaryTerrainLight);
}

void CreateSparks(OBJECT* o, const std::array<Piece, 7>& pieces, int count)
{
    const float seconds = FrameSeconds();
    float wished = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const Piece& piece = pieces[i];
        if (piece.level < T::SparkMinLevel)
            continue;
        wished += T::SparksPerSecond[piece.level >= 15 ? 1 : 0] * static_cast<float>(piece.positionCount);
    }
    if (wished <= 0.0f)
        return;
    g_sparkDemandThisFrame += wished;

    const float share = SparkShare();
    for (int i = 0; i < count; ++i)
    {
        const Piece& piece = pieces[i];
        if (piece.level < T::SparkMinLevel)
            continue;
        const float perBone = T::SparksPerSecond[piece.level >= 15 ? 1 : 0] * share * seconds;
        vec3_t light;
        for (int axis = 0; axis < 3; ++axis)
            light[axis] = (piece.color[axis] * (1.0f - T::SparkWhite) + T::SparkWhite) * T::SparkBrightness;
        for (int k = 0; k < piece.positionCount; ++k)
        {
            // Whole sparks this frame, and the rest by chance.
            float wish = perBone;
            while (wish > 0.0f)
            {
                if (wish >= 1.0f || Random01() < wish)
                {
                    vec3_t position;
                    VectorCopy(piece.positions[k], position);
                    CreateParticle(BITMAP_FLARE, position, o->Angle, light, 10, T::SparkScale);
                }
                wish -= 1.0f;
            }
        }
    }
}

// ---- The flash after an enchant to +14 or +15 ----

struct Flash
{
    bool active = false;
    int key = 0;
    int tier = 0; // 0 = +13, 1 = +14, 2 = +15
    double start = 0.0;
    bool burstDone = false;
};

std::array<Flash, 8> g_flashes{};

void StartFlash(int key, int level)
{
    Flash* slot = nullptr;
    for (Flash& flash : g_flashes)
    {
        if (flash.active && flash.key == key)
        {
            slot = &flash;
            break;
        }
    }
    if (slot == nullptr)
    {
        for (Flash& flash : g_flashes)
        {
            if (!flash.active)
            {
                slot = &flash;
                break;
            }
        }
    }
    if (slot == nullptr)
    {
        // All busy: replace the oldest.
        slot = &*std::min_element(g_flashes.begin(), g_flashes.end(),
                                  [](const Flash& a, const Flash& b) { return a.start < b.start; });
    }
    slot->active = true;
    slot->key = key;
    slot->tier = std::clamp(level - 13, 0, 2); // the tuning tables keep a +13 row, unused since only +14/+15 flash
    slot->start = WorldTime;
    slot->burstDone = false;
}

// The flash's colour: the glow colour of the character's highest piece, gold without one.
void FlashColor(CHARACTER* c, vec3_t color)
{
    int best = -1;
    int bestType = -1;
    for (int i = 0; i < 2; ++i)
    {
        if (c->Weapon[i].Type != -1 && c->Weapon[i].Level > best)
        {
            best = c->Weapon[i].Level;
            bestType = c->Weapon[i].Type;
        }
    }
    for (const ArmourPiece& armour : ArmourPieces)
    {
        const PART_t& part = c->BodyPart[armour.bodyPart];
        if (part.Type != -1 && part.Level > best)
        {
            best = part.Level;
            bestType = part.Type;
        }
    }
    if (best >= 13 && bestType != -1)
    {
        GlowColor(bestType, color);
        return;
    }
    Vector(T::FlashDefaultColor[0], T::FlashDefaultColor[1], T::FlashDefaultColor[2], color);
}

void Burst(CHARACTER* c, OBJECT* o, int tier)
{
    for (int i = 0; i < T::BurstFlares[tier]; ++i)
        CreateJoint(BITMAP_FLARE, o->Position, o->Position, o->Angle, 0, o, 40, 2);
    if (T::BurstMasterFlares[tier] > 0)
    {
        CreateJoint(BITMAP_FLARE, o->Position, o->Position, o->Angle, 45, o, 80, 2);
        for (int i = 0; i < T::BurstMasterFlares[tier]; ++i)
            CreateJoint(BITMAP_FLARE, o->Position, o->Position, o->Angle, 46, o, 80, 2);
    }
    CreateEffect(BITMAP_MAGIC + 1, o->Position, o->Angle, o->Light, 0, o);
    if (T::FlashSound)
        PlayBuffer(SOUND_JEWEL01, o);
    (void)c;
}

void RenderFlash(CHARACTER* c, OBJECT* o, Flash& flash)
{
    const int tier = flash.tier;
    const float elapsed = static_cast<float>((WorldTime - flash.start) / 1000.0);
    const float duration = T::FlashSeconds[tier];
    if (elapsed < 0.0f || elapsed >= duration)
    {
        flash.active = false;
        return;
    }

    if (!flash.burstDone)
    {
        flash.burstDone = true;
        Burst(c, o, tier);
    }

    float strength = 1.0f;
    if (elapsed < T::FlashFadeIn)
        strength = elapsed / T::FlashFadeIn;
    else if (elapsed > duration - T::FlashFadeOut)
        strength = (duration - elapsed) / T::FlashFadeOut;
    strength = std::clamp(strength, 0.0f, 1.0f);
    // A little flicker keeps it alive.
    strength *= 0.9f + 0.1f * sinf(static_cast<float>(WorldTime) * 0.03f);

    vec3_t color;
    FlashColor(c, color);
    vec3_t core, glow;
    for (int axis = 0; axis < 3; ++axis)
    {
        core[axis] = (color[axis] * (1.0f - T::PillarCoreWhite) + T::PillarCoreWhite) * T::PillarCoreBrightness[tier] *
                     strength;
        glow[axis] = color[axis] * T::PillarGlowBrightness[tier] * strength;
    }

    // The column rises from the feet as it fades in.
    const float height = T::PillarHeight[tier] * std::min(1.0f, elapsed / std::max(T::FlashFadeIn, 0.01f));
    for (float z = 0.0f; z <= height; z += T::PillarStep)
    {
        vec3_t position;
        Vector(o->Position[0], o->Position[1], o->Position[2] + z, position);
        // The column thins towards the top.
        const float taper = 1.0f - 0.5f * (z / std::max(T::PillarHeight[tier], 1.0f));
        CreateSprite(BITMAP_LIGHT, position, T::PillarGlowScale[tier] * taper, glow, o);
        CreateSprite(BITMAP_LIGHT, position, T::PillarCoreScale[tier] * taper, core, o);
    }

    vec3_t circle;
    VectorScale(color, T::GroundCircleBrightness[tier] * strength, circle);
    EnableAlphaBlend();
    RenderTerrainAlphaBitmap(BITMAP_MAGIC_ZIN, o->Position[0], o->Position[1], T::GroundCircle[tier],
                             T::GroundCircle[tier], circle, static_cast<float>(WorldTime) * 0.006f);

    vec3_t ground;
    VectorScale(color, T::FlashGroundLight[tier] * strength, ground);
    AddTerrainLight(o->Position[0], o->Position[1], ground, T::FlashGroundRange[tier], PrimaryTerrainLight);
}

bool TakeMessage(const std::wstring& text)
{
    if (!text.starts_with(GameLogic::Items::EnchantFlashLine::Prefix))
        return false;
    // Ours either way: a bad line stays out of the chat too. Classic shows no flash.
    const auto flash = GameLogic::Items::EnchantFlashLine::Parse(text);
    if (!flash || !Enabled())
        return true;
    const int index = FindCharacterIndex(flash->key);
    if (index >= MAX_CHARACTERS_CLIENT)
        return true;
    if (!CharactersClient[index].Object.Visible)
        return true;
    StartFlash(flash->key, flash->level);
    return true;
}

void ForgetFlashes()
{
    for (Flash& flash : g_flashes)
        flash.active = false;
}

// The enchant flash lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"EnchantFx",
    .prefixes = {GameLogic::Items::EnchantFlashLine::Prefix},
    .take = TakeMessage,
    .enteredWorld = ForgetFlashes,
});
} // namespace

bool Enabled()
{
    return Render::Textures::Hd::Enabled();
}

float ChromeShare(int level)
{
    if (level >= T::SparkMinLevel)
        return T::ChromeShareHigh;
    if (level >= T::AuraMinLevel)
        return T::ChromeShareMid;
    return T::ChromeShareLow;
}

float BodyLight()
{
    return T::BodyLight;
}

void RenderCharacter(CHARACTER* c)
{
    if (c == nullptr || !Enabled() || (SceneFlag != MAIN_SCENE && SceneFlag != CHARACTER_SCENE))
        return;
    OBJECT* o = &c->Object;
    if (!o->Visible)
        return;

    StartFrame();

    if (SceneFlag == MAIN_SCENE)
    {
        for (Flash& flash : g_flashes)
        {
            if (flash.active && flash.key == c->Key)
                RenderFlash(c, o, flash);
        }
    }

    // The aura and the sparks: players in their own shape, seen, with the glow on.
    if (o->Kind != KIND_PLAYER || o->Type != MODEL_PLAYER || g_pOption->GetRenderLevel() == 0 ||
        g_isCharacterBuff(o, eBuff_Cloaking) || gMapManager.InChaosCastle())
        return;

    BMD* b = &Models[o->Type];
    std::array<Piece, 7> pieces{};
    const int count = CollectPieces(c, o, b, pieces);
    if (count == 0)
        return;
    RenderAura(c, o, b, pieces, count);
    CreateSparks(o, pieces, count);
}
} // namespace Render::Items::EnchantFx
