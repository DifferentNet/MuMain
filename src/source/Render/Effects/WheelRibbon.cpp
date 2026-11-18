#include "stdafx.h"

#include "Render/Effects/WheelRibbon.h"

#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/RenderUtils.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <array>

namespace Render::Effects::WheelRibbon
{

namespace
{
constexpr int kVerticesPerQuad = 4;
constexpr int kTexture = BITMAP_BLUR + 2;
// V across the band: 0 on the inner edge, 1 on the outer.
constexpr float kInnerV = 0.0f;
constexpr float kOuterV = 1.0f;

struct EdgePoint
{
    float inner[3];
    float outer[3];
    float u;
    std::uint32_t color;
};

unsigned int s_statsFrame = 0;
int s_current = 0;
int s_last = 0;

// The bands of the frame; RenderEffects runs more than once a frame, so they start over when the frame changes.
int& CurrentFrameArcs()
{
    if (s_statsFrame != RENDERED_FRAME)
    {
        s_last = s_statsFrame + 1 == RENDERED_FRAME ? s_current : 0;
        s_current = 0;
        s_statsFrame = RENDERED_FRAME;
    }
    return s_current;
}

// The same test as Move_MODEL_SKILL_WHEEL2.
bool HoldsSpear(const OBJECT& owner)
{
    return owner.Weapon >= ITEM_SPEAR && owner.Weapon < ITEM_SPEAR + MAX_ITEM_INDEX;
}

mu::Vertex3D Vertex(const float position[3], const float u, const float v, const std::uint32_t color)
{
    return mu::Vertex3D{position[0], position[1], position[2], 0.f, 0.f, 1.f, u, v, color};
}
} // namespace

void Draw(const OBJECT& ghost, const int piecesPerSpan)
{
    const float frameFactor = FPS_ANIMATION_FACTOR;
    const float arc = Arc(ghost.LifeTime, frameFactor);
    const int pieces = Pieces(arc, piecesPerSpan);
    if (pieces <= 0 || ghost.Owner == nullptr)
    {
        return;
    }
    const OBJECT& owner = *ghost.Owner;
    const float radius = Radius(HoldsSpear(owner));
    const float head = HeadDegrees(ghost.Angle[2], frameFactor);
    const float fade = EndFade(ghost.LifeTime);

    std::array<EdgePoint, kMaxPieces + 1> edge;
    for (int i = 0; i <= pieces; ++i)
    {
        const float along = static_cast<float>(i) / static_cast<float>(pieces);
        const vec3_t angle = {ghost.Angle[0], ghost.Angle[1], head + arc * along};
        float matrix[3][4];
        AngleMatrix(angle, matrix);
        EdgePoint& point = edge[i];
        Point(matrix, owner.Position, radius - kHalfWidth, point.inner);
        Point(matrix, owner.Position, radius + kHalfWidth, point.outer);
        point.u = along;
        const float light = Light(along, fade);
        point.color = mu::PackABGR(kColor[0] * light, kColor[1] * light, kColor[2] * light, 1.f);
    }

    std::array<mu::Vertex3D, kMaxPieces * kVerticesPerQuad> quads;
    for (int i = 0; i < pieces; ++i)
    {
        const EdgePoint& a = edge[i];
        const EdgePoint& b = edge[i + 1];
        mu::Vertex3D* quad = &quads[static_cast<std::size_t>(i) * kVerticesPerQuad];
        quad[0] = Vertex(a.outer, a.u, kOuterV, a.color);
        quad[1] = Vertex(a.inner, a.u, kInnerV, a.color);
        quad[2] = Vertex(b.inner, b.u, kInnerV, b.color);
        quad[3] = Vertex(b.outer, b.u, kOuterV, b.color);
    }
    EnableAlphaBlend();
    BindTexture(kTexture);
    mu::GetRenderer().RenderQuad3D(
        std::span<const mu::Vertex3D>(quads.data(), static_cast<std::size_t>(pieces) * kVerticesPerQuad),
        static_cast<std::uint32_t>(kTexture));
    ++CurrentFrameArcs();
}

int LastFrameArcs()
{
    if (s_statsFrame == RENDERED_FRAME)
    {
        return s_last;
    }
    return s_statsFrame + 1 == RENDERED_FRAME ? s_current : 0;
}

} // namespace Render::Effects::WheelRibbon
