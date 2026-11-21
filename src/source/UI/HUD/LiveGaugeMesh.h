#pragma once

// The live HUD's gem meshes (UI/HUD/LiveHud.h), built into quads for <additive-mesh>
// (Render/RmlUi/RmlAdditiveMeshElement.h): four vertices each, top-left, bottom-left, bottom-right, top-right,
// positions in pixels of the mesh element's border box, u and v 0..1 of the gem's art. Pure: no RmlUi.

#include "Render/Renderer/Overlay2DRecorder.h"
#include "UI/HUD/LiveGaugeMath.h"

#include <cstdint>
#include <span>
#include <vector>

namespace UI::HUD::LiveGauge
{
using Vertex = Render::Renderer::RecordedVertex;

// A rectangle in pixels of the mesh element.
struct Box
{
    float left = 0.f;
    float top = 0.f;
    float width = 0.f;
    float height = 0.f;

    bool operator==(const Box&) const = default;
};

// A gem's silhouette sampled at its column edges: 16 even columns across it plus the outline's corners, and the
// silhouette's top and bottom at each edge.
struct GemShape
{
    Box box;
    float zoom = 1.f;
    std::vector<float> xs;
    std::vector<float> tops;
    std::vector<float> bottoms;
    float topY = 0.f;
    float bottomY = 0.f;

    // The liquid's flat line at `level`: 0 is the silhouette's bottom, 1 its top.
    [[nodiscard]] float LevelY(float level) const;
    // u or v of the gem's art for a point of the box, the art zoomed about its centre.
    [[nodiscard]] float U(float x) const;
    [[nodiscard]] float V(float y) const;
};

// `outline` (0..1 of `box`, convex: LiveGauge::ParseOutline()) sampled into `shape`, whose vectors are reused.
void ShapeGem(const Box& box, std::span<const OutlinePoint> outline, float zoom, GemShape& shape);

// The liquid's wave: LiveGauge::WaveOffset() at `time`, `amplitude` in pixels (the slosh already in it).
struct Wave
{
    double time = 0.0;
    float period = 1.f;
    float amplitude = 0.f;
};

// The liquid's surface at each of the shape's column edges: the level's line plus the wave, kept inside the
// silhouette. `surface` is reused.
void ShapeSurface(const GemShape& shape, float level, const Wave& wave, std::vector<float>& surface);

// The art's brightness in each part of a column.
struct BodyShades
{
    float empty = 0.30f;
    float ghost = 0.60f;
    float fill = 1.f;
};

// The gem's body, drawn as an image (SetMesh with additive off) of the gem's art. Each column is three stacked quads
// that never overlap: empty from the silhouette's top to the ghost, the ghost down to the surface, and the liquid from
// the surface to the silhouette's bottom. The ghost's line is kept between the silhouette's top and the surface, so a
// `ghostLevel` of 0 shows no ghost. An empty part is left out. `out` is cleared first.
void BuildOrbBody(const GemShape& shape, std::span<const float> surface, float ghostLevel, const BodyShades& shades,
                  std::vector<Vertex>& out);

// The light over the gem, added with the gem's art (SetMesh, additive): dark pixels add nothing, so it keeps to the
// art.
struct ShineInputs
{
    // A band this many pixels under the surface, its top `crestTint` bright, its bottom dark.
    float crestHeight = 0.f;
    float crestTint = 0.45f;
    // A static glass highlight at the top left, this bright at its centre.
    float highlightPeak = 0.12f;
    // A diagonal band crossing the gem left to right while this runs 0..1; none below 0.
    float sheenPhase = -1.f;
    // Added over the whole gem.
    float brighten = 0.f;
};
// `out` is cleared first; a quad that adds nothing is left out.
void BuildOrbShine(const GemShape& shape, std::span<const float> surface, const ShineInputs& inputs,
                   std::vector<Vertex>& out);

// The socket's rim colours, 0xAABBGGRR.
struct RimColours
{
    std::uint32_t outer = 0u;
    std::uint32_t inner = 0u;
};

// The gem's socket, drawn plain (SetPlainMesh, blended): a rim `width` pixels wide round the outline of `box`,
// `outer` at its outside shading to `inner` at the gem's edge. `out` is cleared first.
void BuildOrbUnder(const Box& box, std::span<const OutlinePoint> outline, float width, const RimColours& colours,
                   std::vector<Vertex>& out);
} // namespace UI::HUD::LiveGauge
