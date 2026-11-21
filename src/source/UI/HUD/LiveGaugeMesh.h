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

// The SD or AG ring round a gem: a band `inner`..`outer` pixels from (centreX, centreY), over `sweepDegrees` from
// `startDegrees` (screen angles: clockwise from 3 o'clock, y down).
struct Ring
{
    float centreX = 0.f;
    float centreY = 0.f;
    float inner = 0.f;
    float outer = 0.f;
    float startDegrees = 0.f;
    float sweepDegrees = 0.f;
};

// The quads a whole sweep is cut into. A part of it keeps the same cuts and ends inside its last quad, so a moving
// end never makes the arc wobble.
inline constexpr int ArcSegments = 40;

// The ring's empty track, drawn plain (SetPlainMesh, blended): `colours.outer` (the bronze) at both of the band's
// edges, shading to `colours.inner` midway. Appended to `out`; nothing for a band with no width.
void AppendArcTrack(const Ring& ring, const RimColours& colours, std::vector<Vertex>& out);

// What a value arc shows: `level` of the sweep, and the damage ghost from there up to `ghost` at `ghostShade`
// (none when `ghost` is not above `level`).
struct ArcInputs
{
    float level = 0.f;
    float ghost = 0.f;
    float ghostShade = 0.60f;
};

// The value arc, drawn as an image (SetMesh, additive off) of the SD or AG bar's art: u across the band, 0.15 at its
// outer edge to 0.85 at its inner one (inside the art's dark side columns), v along the whole sweep. `out` is cleared
// first; nothing at level 0 without a ghost.
void BuildArc(const Ring& ring, const ArcInputs& inputs, std::vector<Vertex>& out);

// The bright cap at the value's end: its last `length` pixels (measured along the band's middle), the same art added
// at `tint` (SetMesh, additive), so the cap shows 1 + `tint` as bright. `out` is cleared first; nothing at level 0.
void BuildArcCap(const Ring& ring, float level, float length, float tint, std::vector<Vertex>& out);

// Where a gem's halo reaches: `radius` pixels from (centreX, centreY), and never below `bottom` (the gem group's
// lower edge, so nothing under the group lights up).
struct HaloShape
{
    float centreX = 0.f;
    float centreY = 0.f;
    float radius = 0.f;
    float bottom = 0.f;
};

// The glow round a gem, drawn plain and added (SetPlainMesh, additive): a fan of quads from the gem's outline
// (`outline` in `box`), `colour` times `strength` there, out to the shape's edge, where it adds nothing. `out` is
// cleared first; nothing at strength 0.
void BuildHalo(const Box& box, std::span<const OutlinePoint> outline, const HaloShape& shape, std::uint32_t colour,
               float strength, std::vector<Vertex>& out);
} // namespace UI::HUD::LiveGauge
