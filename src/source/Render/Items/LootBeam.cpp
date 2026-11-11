#include "stdafx.h"
#include "Render/Items/LootBeam.h"

#include <array>
#include <cmath>

#include "Camera/CameraState.h"
#include "Core/Globals/_TextureIndex.h"
#include "Engine/Object/ZzzObject.h"
#include "GameLogic/Items/ItemCategories.h"
#include "GameLogic/Items/LootFilterRuntime.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/RenderUtils.h"
#include "Render/Textures/ZzzOpenglUtil.h"

namespace Render::Items::LootBeam
{
namespace
{
using Tint = std::array<float, 3>;

// World units (one terrain tile is 100): almost three tiles tall, wider at the foot and narrowing upward.
constexpr float BeamHeight = 280.f;
constexpr float BeamBaseLift = 10.f;
// Points closer than this to the camera plane are skipped, as RenderSprite does.
constexpr float NearPlaneZ = -1.f;
// The light sprite is a round glow: its middle row along the foot, a row near its edge at the top, so the beam
// fades upward.
constexpr float SpriteCentreV = 0.5f;
constexpr float SpriteTopV = 0.08f;
// A slow pulse between PulseLow and PulseLow + PulseRange; each item has its own phase.
constexpr float PulseSpeed = 0.004f;
constexpr float PulsePhasePerItem = 1.7f;
constexpr float PulseLow = 0.7f;
constexpr float PulseRange = 0.3f;

// The beam is two additive layers: a wide soft glow and a narrow bright core. The core takes the sprite's middle
// columns only, so it is bright right across, and is drawn twice for strength.
struct Layer
{
    float footHalfWidth;
    float topHalfWidth;
    float uLow;
    float uHigh;
    float brightness;
    int passes;
};
constexpr Layer GlowLayer{40.f, 16.f, 0.f, 1.f, 0.55f, 1};
constexpr Layer CoreLayer{20.f, 6.f, 0.3f, 0.7f, 1.f, 2};

constexpr Tint AncientTint{0.35f, 0.55f, 1.0f};
constexpr Tint ExcellentTint{0.3f, 1.0f, 0.4f};
constexpr Tint JewelTint{1.0f, 0.85f, 0.25f};
constexpr Tint OtherTint{1.0f, 0.9f, 0.7f};

Tint TintOf(const ITEM& item)
{
    if (item.AncientDiscriminator != 0)
        return AncientTint;
    if (GameLogic::Items::LootFilterRuntime::IsExcellent(item))
        return ExcellentTint;
    if (GameLogic::Items::IsJewelItem(&item))
        return JewelTint;
    return OtherTint;
}

void RenderBeam(int index, const ITEM_t& entry)
{
    const OBJECT& o = entry.Object;
    vec3_t bottom{o.Position[0], o.Position[1], o.Position[2] + BeamBaseLift};
    vec3_t top{bottom[0], bottom[1], bottom[2] + BeamHeight};

    vec3_t viewBottom, viewTop;
    VectorTransform(bottom, g_Camera.Matrix, viewBottom);
    VectorTransform(top, g_Camera.Matrix, viewTop);
    if (viewBottom[2] >= NearPlaneZ || viewTop[2] >= NearPlaneZ)
        return;

    // Widen across the beam's own direction on screen, so it stays a strip at any camera pitch.
    const float dx = viewTop[0] - viewBottom[0];
    const float dy = viewTop[1] - viewBottom[1];
    const float length = std::sqrt(dx * dx + dy * dy);
    const float sideX = length > 0.f ? -dy / length : 1.f;
    const float sideY = length > 0.f ? dx / length : 0.f;

    const float pulse =
        PulseLow + PulseRange * (0.5f + 0.5f * std::sin(static_cast<float>(WorldTime) * PulseSpeed + index * PulsePhasePerItem));
    const Tint tint = TintOf(entry.Item);

    for (const Layer& layer : {GlowLayer, CoreLayer})
    {
        const float strength = pulse * layer.brightness;
        const std::uint32_t color = mu::PackABGR(tint[0] * strength, tint[1] * strength, tint[2] * strength, 1.f);
        const float footX = sideX * layer.footHalfWidth;
        const float footY = sideY * layer.footHalfWidth;
        const float topX = sideX * layer.topHalfWidth;
        const float topY = sideY * layer.topHalfWidth;
        const mu::Vertex3D vertices[4] = {
            {viewBottom[0] - footX, viewBottom[1] - footY, viewBottom[2], 0.f, 0.f, 0.f, layer.uLow, SpriteCentreV, color},
            {viewBottom[0] + footX, viewBottom[1] + footY, viewBottom[2], 0.f, 0.f, 0.f, layer.uHigh, SpriteCentreV, color},
            {viewTop[0] + topX, viewTop[1] + topY, viewTop[2], 0.f, 0.f, 0.f, layer.uHigh, SpriteTopV, color},
            {viewTop[0] - topX, viewTop[1] - topY, viewTop[2], 0.f, 0.f, 0.f, layer.uLow, SpriteTopV, color},
        };
        for (int pass = 0; pass < layer.passes; ++pass)
            mu::GetRenderer().RenderQuad3D(vertices, static_cast<std::uint32_t>(BITMAP_LIGHT));
    }
}
} // namespace

void RenderAll()
{
    namespace Runtime = GameLogic::Items::LootFilterRuntime;
    if (!Runtime::MarkerOn())
        return;

    bool started = false;
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        const ITEM_t& entry = ::Items[i];
        if (!entry.Object.Live || !entry.Object.Visible || !Runtime::IsHighlighted(i))
            continue;
        if (!started)
        {
            BeginSprite();
            EnableAlphaBlend(); // additive, no depth writes: the beam glows and never hides what is behind it
            BindTexture(BITMAP_LIGHT);
            started = true;
        }
        RenderBeam(i, entry);
    }
    if (!started)
        return;
    EndSprite();
    DisableAlphaBlend();
}
} // namespace Render::Items::LootBeam
