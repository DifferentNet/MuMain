#include "stdafx.h"

#include "Render/Effects/GroundMarks.h"

#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/RenderUtils.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "GameLogic/Events/Cinematic/CDirection.h"
#include "World/GameMaps/GMHellas.h"
#include "World/MapInfra/MapManager.h"

#include <vector>

namespace Render::Effects::GroundMarks
{

namespace
{
static_assert(kTileSize == TERRAIN_SCALE, "GroundMarks.h tiles must be the terrain's");

// A mark lies this far above the ground, as the classic terrain bitmaps do.
constexpr float kHeightAboveGround = 5.f;
// TestFrustrum2D's margin (tile units, as the boids and characters use it, a little wider).
constexpr float kFrustumRange = -40.f;
// The faintest mark drawn.
constexpr float kMinAlpha = 1.f / 255.f;
// The frost tint (MU's pale ice blue).
constexpr float kFrostTint[3] = {0.55f, 0.75f, 1.f};
constexpr int kVerticesPerQuad = 4;

MarkPool s_pool;
// The map the marks lie on.
int s_world = -1;
// The quads of one kind, reused frame to frame (main thread only).
std::vector<mu::Vertex3D> s_quads;

// Whether a tile is ground a mark may lie on: inside the map, not a hole and not water.
bool IsGroundTile(const int tileX, const int tileY)
{
    if (tileX < 0 || tileY < 0 || tileX >= TERRAIN_SIZE_MASK || tileY >= TERRAIN_SIZE_MASK)
    {
        return false;
    }
    const WORD wall = TerrainWall[TERRAIN_INDEX(tileX, tileY)];
    return (wall & TW_NOGROUND) != TW_NOGROUND && (wall & TW_WATER) != TW_WATER;
}

// Whether the map draws a ground marks can lie on: not the water maps (Kalima, their ground is drawn
// after the effects), Atlans, Icarus (no terrain) or Kanturu's Maya scene (terrain not drawn then).
bool MapTakesMarks()
{
    const int world = gMapManager.WorldActive;
    if (IsWaterTerrain() || world == WD_7ATLANSE || world == WD_10HEAVEN)
    {
        return false;
    }
    return !(world == WD_39KANTURU_3RD && g_Direction.m_CKanturu.IsMayaScene());
}

void FollowMap()
{
    if (s_world != gMapManager.WorldActive)
    {
        s_pool.Clear();
        s_world = gMapManager.WorldActive;
    }
}

// The texture of a kind, or -1 when it has none loaded.
int TextureOf(const Kind kind)
{
    switch (kind)
    {
    case Kind::Scorch:
    case Kind::LargeScorch:
        return BITMAP_CRATER;
    case Kind::Frost:
        return BITMAP_CLUD64;
    }
    return -1;
}

std::uint32_t ColorOf(const Kind kind, const float alpha)
{
    if (kind == Kind::Frost)
    {
        // Added (GL_ONE, GL_ONE): the brightness carries the fade.
        return mu::PackABGR(kFrostTint[0] * alpha, kFrostTint[1] * alpha, kFrostTint[2] * alpha, alpha);
    }
    return mu::PackABGR(1.f, 1.f, 1.f, alpha);
}

mu::Vertex3D CornerAt(const Mark& mark, const float x, const float y, const std::uint32_t color)
{
    const float u = (x - (mark.x - mark.halfW)) / (2.f * mark.halfW);
    const float v = (y - (mark.y - mark.halfH)) / (2.f * mark.halfH);
    const UV uv = MirroredUV(u, v, mark.mirror);
    const float z = RequestTerrainHeight(x, y) + kHeightAboveGround;
    return mu::Vertex3D{x, y, z, 0.f, 0.f, 1.f, uv.u, uv.v, color};
}

// One quad per ground tile under the mark, clipped to the mark.
void AppendMark(const Mark& mark, const float alpha, std::vector<mu::Vertex3D>& quads)
{
    if (mark.halfW <= 0.f || mark.halfH <= 0.f)
    {
        return;
    }
    const std::uint32_t color = ColorOf(mark.kind, alpha);
    const TileSpan tilesX = TilesUnder(mark.x, mark.halfW);
    const TileSpan tilesY = TilesUnder(mark.y, mark.halfH);
    for (int tileY = tilesY.first; tileY <= tilesY.last; ++tileY)
    {
        const Span spanY = ClipToTile(mark.y, mark.halfH, tileY);
        if (spanY.hi <= spanY.lo)
        {
            continue;
        }
        for (int tileX = tilesX.first; tileX <= tilesX.last; ++tileX)
        {
            const Span spanX = ClipToTile(mark.x, mark.halfW, tileX);
            if (spanX.hi <= spanX.lo || !IsGroundTile(tileX, tileY))
            {
                continue;
            }
            quads.push_back(CornerAt(mark, spanX.lo, spanY.lo, color));
            quads.push_back(CornerAt(mark, spanX.hi, spanY.lo, color));
            quads.push_back(CornerAt(mark, spanX.hi, spanY.hi, color));
            quads.push_back(CornerAt(mark, spanX.lo, spanY.hi, color));
        }
    }
}

void DrawKind(const Kind kind)
{
    const int texture = TextureOf(kind);
    if (texture < 0)
    {
        return;
    }
    s_quads.clear();
    for (int i = 0; i < s_pool.Count(); ++i)
    {
        const Mark& mark = s_pool.At(i);
        if (mark.kind != kind)
        {
            continue;
        }
        const float alpha = Alpha(mark.age, kind);
        if (alpha < kMinAlpha || !TestFrustrum2D(mark.x * 0.01f, mark.y * 0.01f, kFrustumRange))
        {
            continue;
        }
        AppendMark(mark, alpha, s_quads);
    }
    if (s_quads.size() < kVerticesPerQuad)
    {
        return;
    }
    if (kind == Kind::Frost)
    {
        EnableAlphaBlend();
    }
    else
    {
        EnableAlphaBlend3();
    }
    BindTexture(texture);
    mu::GetRenderer().RenderQuad3D(s_quads, static_cast<std::uint32_t>(texture));
}
} // namespace

bool Add(const Source source, const float x, const float y)
{
    if (!SkillFx::Enabled(SkillFx::Feature::Decals) || !MapTakesMarks())
    {
        return false;
    }
    const Look look = LookOf(source);
    // No texture loaded: no mark.
    if (TextureOf(look.kind) < 0 || !IsGroundTile(FloorToInt(x / TERRAIN_SCALE), FloorToInt(y / TERRAIN_SCALE)))
    {
        return false;
    }
    FollowMap();
    Mark mark;
    mark.x = x;
    mark.y = y;
    mark.halfW = look.halfW;
    mark.halfH = look.halfH;
    mark.kind = look.kind;
    mark.mirror = MirrorFor(x, y, look.halfW, look.halfH);
    return s_pool.Add(mark, SkillFx::ActiveMarkBudget());
}

void Update()
{
    FollowMap();
    s_pool.Update(FPS_ANIMATION_FACTOR, MapTakesMarks() ? SkillFx::ActiveMarkBudget() : 0);
}

void Draw()
{
    if (s_pool.Count() == 0 || !MapTakesMarks())
    {
        return;
    }
    EnableDepthTest();
    for (int index = 0; index < kKindCount; ++index)
    {
        DrawKind(static_cast<Kind>(index));
    }
}

void Clear()
{
    s_pool.Clear();
}

int Count()
{
    return s_pool.Count();
}

} // namespace Render::Effects::GroundMarks
