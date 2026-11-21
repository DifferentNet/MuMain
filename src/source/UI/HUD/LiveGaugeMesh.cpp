#include "UI/HUD/LiveGaugeMesh.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace UI::HUD::LiveGauge
{
namespace
{
constexpr int EvenColumns = 16;
// Edges closer than this are one edge, and a part this thin is left out.
constexpr float PixelEpsilon = 0.01f;

// The shine's grid: rows per column, so the highlight and the sheen bend smoothly.
constexpr int ShineRows = 8;
// The glass highlight's centre and reach, 0..1 of the gem's box.
constexpr float HighlightX = 0.30f;
constexpr float HighlightY = 0.22f;
constexpr float HighlightRadius = 0.45f;
// The sheen: a band along x + Slant * y, HalfWidth either side of its centre line, Peak bright on it.
constexpr float SheenSlant = 0.6f;
constexpr float SheenHalfWidth = 0.16f;
constexpr float SheenPeak = 0.30f;
// A vertex below this adds nothing visible.
constexpr float LightEpsilon = 1.f / 255.f;

constexpr float ChannelMax = 255.f;
constexpr std::uint32_t OpaqueAlpha = 0xFF000000u;

constexpr float DegreesToRadians = 0.017453292519943295f;
// Angles closer than this are one angle.
constexpr float AngleEpsilon = 1.0e-4f;
// The value arc's art (the SD and AG bars, 16x39 tubes): u across the band, inside the tube's dark side columns;
// v along the sweep, inside its dark first and last rows.
constexpr float ArcOuterU = 0.15f;
constexpr float ArcInnerU = 0.85f;
constexpr float ArcFirstV = 0.05f;
constexpr float ArcLastV = 0.95f;
// The halo's even cuts round the gem; the outline's corners are cuts too.
constexpr int HaloSegments = 48;
constexpr float FullTurnDegrees = 360.f;

// A grey at `shade` (0..1), opaque.
std::uint32_t Grey(float shade)
{
    const auto channel = static_cast<std::uint32_t>(std::lround(std::clamp(shade, 0.f, 1.f) * ChannelMax));
    return OpaqueAlpha | (channel << 16) | (channel << 8) | channel;
}

// Every channel of `abgr`, alpha too, times `strength` (0..1).
std::uint32_t Scaled(std::uint32_t abgr, float strength)
{
    const float share = std::clamp(strength, 0.f, 1.f);
    std::uint32_t scaled = 0u;
    for (const int shift : {0, 8, 16, 24})
    {
        const auto channel = static_cast<float>((abgr >> shift) & 0xFFu);
        scaled |= static_cast<std::uint32_t>(std::lround(channel * share)) << shift;
    }
    return scaled;
}

// One column part from (x0, upper0..lower0) to (x1, upper1..lower1), u and v from the shape; left out when it has no
// height at either edge.
void PushColumnQuad(const GemShape& shape, float x0, float upper0, float lower0, float x1, float upper1, float lower1,
                    std::uint32_t colour, std::vector<Vertex>& out)
{
    if (lower0 - upper0 <= PixelEpsilon && lower1 - upper1 <= PixelEpsilon)
        return;
    out.push_back({x0, upper0, shape.U(x0), shape.V(upper0), colour});
    out.push_back({x0, lower0, shape.U(x0), shape.V(lower0), colour});
    out.push_back({x1, lower1, shape.U(x1), shape.V(lower1), colour});
    out.push_back({x1, upper1, shape.U(x1), shape.V(upper1), colour});
}

float HighlightAt(float xn, float yn, float peak)
{
    const float distance = std::hypot(xn - HighlightX, yn - HighlightY) / HighlightRadius;
    if (distance >= 1.f)
        return 0.f;
    const float falloff = 1.f - distance;
    return peak * falloff * falloff;
}

float SheenAt(float xn, float yn, float phase)
{
    if (phase < 0.f)
        return 0.f;
    // The centre line runs from just before the gem's top-left corner to just past its bottom-right one.
    const float travel = 1.f + SheenSlant + 2.f * SheenHalfWidth;
    const float centre = -SheenHalfWidth + std::min(phase, 1.f) * travel;
    const float distance = std::fabs(xn + SheenSlant * yn - centre) / SheenHalfWidth;
    return distance >= 1.f ? 0.f : SheenPeak * (1.f - distance);
}

float LightAt(const GemShape& shape, const ShineInputs& inputs, float x, float y)
{
    const float xn = (x - shape.box.left) / shape.box.width;
    const float yn = (y - shape.box.top) / shape.box.height;
    const float light =
        HighlightAt(xn, yn, inputs.highlightPeak) + SheenAt(xn, yn, inputs.sheenPhase) + inputs.brighten;
    return std::clamp(light, 0.f, 1.f);
}

// One cell of the shine's grid, each corner lit on its own; left out when it adds nothing.
void PushLitQuad(const GemShape& shape, const ShineInputs& inputs, float x0, float upper0, float lower0, float x1,
                 float upper1, float lower1, std::vector<Vertex>& out)
{
    const float lights[] = {LightAt(shape, inputs, x0, upper0), LightAt(shape, inputs, x0, lower0),
                            LightAt(shape, inputs, x1, lower1), LightAt(shape, inputs, x1, upper1)};
    if (*std::max_element(std::begin(lights), std::end(lights)) < LightEpsilon)
        return;
    out.push_back({x0, upper0, shape.U(x0), shape.V(upper0), Grey(lights[0])});
    out.push_back({x0, lower0, shape.U(x0), shape.V(lower0), Grey(lights[1])});
    out.push_back({x1, lower1, shape.U(x1), shape.V(lower1), Grey(lights[2])});
    out.push_back({x1, upper1, shape.U(x1), shape.V(upper1), Grey(lights[3])});
}

void PushGlassAndSheen(const GemShape& shape, const ShineInputs& inputs, std::vector<Vertex>& out)
{
    for (std::size_t i = 0; i + 1 < shape.xs.size(); ++i)
    {
        const float x0 = shape.xs[i];
        const float x1 = shape.xs[i + 1];
        const float height0 = shape.bottoms[i] - shape.tops[i];
        const float height1 = shape.bottoms[i + 1] - shape.tops[i + 1];
        for (int row = 0; row < ShineRows; ++row)
        {
            const float from = static_cast<float>(row) / ShineRows;
            const float to = static_cast<float>(row + 1) / ShineRows;
            PushLitQuad(shape, inputs, x0, shape.tops[i] + from * height0, shape.tops[i] + to * height0, x1,
                        shape.tops[i + 1] + from * height1, shape.tops[i + 1] + to * height1, out);
        }
    }
}

void PushCrest(const GemShape& shape, std::span<const float> surface, const ShineInputs& inputs,
               std::vector<Vertex>& out)
{
    if (inputs.crestHeight <= 0.f || inputs.crestTint <= 0.f)
        return;
    const std::uint32_t bright = Grey(inputs.crestTint);
    const std::uint32_t dark = Grey(0.f);
    for (std::size_t i = 0; i + 1 < shape.xs.size(); ++i)
    {
        const float x0 = shape.xs[i];
        const float x1 = shape.xs[i + 1];
        const float lower0 = std::min(surface[i] + inputs.crestHeight, shape.bottoms[i]);
        const float lower1 = std::min(surface[i + 1] + inputs.crestHeight, shape.bottoms[i + 1]);
        if (lower0 - surface[i] <= PixelEpsilon && lower1 - surface[i + 1] <= PixelEpsilon)
            continue;
        out.push_back({x0, surface[i], shape.U(x0), shape.V(surface[i]), bright});
        out.push_back({x0, lower0, shape.U(x0), shape.V(lower0), dark});
        out.push_back({x1, lower1, shape.U(x1), shape.V(lower1), dark});
        out.push_back({x1, surface[i + 1], shape.U(x1), shape.V(surface[i + 1]), bright});
    }
}

struct Point
{
    float x = 0.f;
    float y = 0.f;
};

Point Outward(Point from, Point to, float orientation)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    if (length <= 0.f)
        return {};
    return {orientation * dy / length, -orientation * dx / length};
}

float Cross(Point a, Point b)
{
    return a.x * b.y - a.y * b.x;
}

Point Direction(float degrees)
{
    const float radians = degrees * DegreesToRadians;
    return {std::cos(radians), std::sin(radians)};
}

// The quads of `ring` from `from` to `to` degrees, `innerRadius`..`outerRadius` from its centre, cut where the
// whole sweep's ArcSegments cut it. Each quad runs outer and inner at its first angle, then inner and outer at its
// last; `innerColour` and `outerColour` are the colours at the two radii. A textured band takes the art's u across
// it and its v along the sweep.
void PushArcBand(const Ring& ring, float innerRadius, float outerRadius, float from, float to,
                 std::uint32_t innerColour, std::uint32_t outerColour, bool textured, std::vector<Vertex>& out)
{
    if (ring.sweepDegrees <= 0.f || outerRadius - innerRadius <= PixelEpsilon || to - from <= AngleEpsilon)
        return;
    const float step = ring.sweepDegrees / ArcSegments;
    const float innerU = textured ? ArcInnerU : 0.f;
    const float outerU = textured ? ArcOuterU : 0.f;
    const auto vAt = [&](float degrees)
    {
        if (!textured)
            return 0.f;
        const float share = std::clamp((degrees - ring.startDegrees) / ring.sweepDegrees, 0.f, 1.f);
        return ArcFirstV + share * (ArcLastV - ArcFirstV);
    };
    const auto push = [&](float degrees, float radius, float u, std::uint32_t colour)
    {
        const Point direction = Direction(degrees);
        out.push_back(
            {ring.centreX + radius * direction.x, ring.centreY + radius * direction.y, u, vAt(degrees), colour});
    };

    const int first = std::clamp(static_cast<int>(std::floor((from - ring.startDegrees) / step)), 0, ArcSegments - 1);
    for (int segment = first; segment < ArcSegments; ++segment)
    {
        const float cutStart = ring.startDegrees + step * static_cast<float>(segment);
        const float cutEnd = ring.startDegrees + step * static_cast<float>(segment + 1);
        const float a0 = std::max(from, cutStart);
        const float a1 = std::min(to, cutEnd);
        if (a1 - a0 > AngleEpsilon)
        {
            push(a0, outerRadius, outerU, outerColour);
            push(a0, innerRadius, innerU, innerColour);
            push(a1, innerRadius, innerU, innerColour);
            push(a1, outerRadius, outerU, outerColour);
        }
        if (cutEnd >= to)
            break;
    }
}

// How far the ray from `centre` along `direction` runs before it leaves the convex polygon `points`; 0 when it
// never crosses it.
float ReachInside(Point centre, Point direction, std::span<const Point> points)
{
    float reach = 0.f;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const Point a = points[i];
        const Point b = points[(i + 1) % points.size()];
        const Point edge{b.x - a.x, b.y - a.y};
        const float denominator = Cross(direction, edge);
        if (std::fabs(denominator) <= std::numeric_limits<float>::epsilon())
            continue;
        const Point relative{a.x - centre.x, a.y - centre.y};
        const float along = Cross(relative, edge) / denominator;
        const float share = Cross(relative, direction) / denominator;
        if (along >= 0.f && share >= -PixelEpsilon && share <= 1.f + PixelEpsilon)
            reach = std::max(reach, along);
    }
    return reach;
}
} // namespace

float GemShape::LevelY(float level) const
{
    return bottomY - std::clamp(level, 0.f, 1.f) * (bottomY - topY);
}

float GemShape::U(float x) const
{
    return 0.5f + ((x - box.left) / box.width - 0.5f) / zoom;
}

float GemShape::V(float y) const
{
    return 0.5f + ((y - box.top) / box.height - 0.5f) / zoom;
}

void ShapeGem(const Box& box, std::span<const OutlinePoint> outline, float zoom, GemShape& shape)
{
    shape.box = box;
    shape.zoom = zoom;
    shape.xs.clear();
    shape.tops.clear();
    shape.bottoms.clear();
    if (outline.size() < 3 || box.width <= 0.f || box.height <= 0.f)
        return;

    const auto pointAt = [&](std::size_t i)
    { return Point{box.left + outline[i].x * box.width, box.top + outline[i].y * box.height}; };

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    shape.topY = std::numeric_limits<float>::max();
    shape.bottomY = std::numeric_limits<float>::lowest();
    for (std::size_t i = 0; i < outline.size(); ++i)
    {
        const Point p = pointAt(i);
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        shape.topY = std::min(shape.topY, p.y);
        shape.bottomY = std::max(shape.bottomY, p.y);
        shape.xs.push_back(p.x);
    }
    for (int column = 0; column <= EvenColumns; ++column)
        shape.xs.push_back(minX + (maxX - minX) * static_cast<float>(column) / EvenColumns);
    std::sort(shape.xs.begin(), shape.xs.end());
    shape.xs.erase(
        std::unique(shape.xs.begin(), shape.xs.end(), [](float a, float b) { return b - a <= PixelEpsilon; }),
        shape.xs.end());

    for (const float x : shape.xs)
    {
        float top = std::numeric_limits<float>::max();
        float bottom = std::numeric_limits<float>::lowest();
        for (std::size_t i = 0; i < outline.size(); ++i)
        {
            const Point a = pointAt(i);
            const Point b = pointAt((i + 1) % outline.size());
            if (x < std::min(a.x, b.x) - PixelEpsilon || x > std::max(a.x, b.x) + PixelEpsilon)
                continue;
            const float span = b.x - a.x;
            const float share = std::fabs(span) <= PixelEpsilon ? 0.f : std::clamp((x - a.x) / span, 0.f, 1.f);
            const float y = a.y + share * (b.y - a.y);
            // A vertical edge reaches both its ends.
            const float otherY = std::fabs(span) <= PixelEpsilon ? b.y : y;
            top = std::min({top, y, otherY});
            bottom = std::max({bottom, y, otherY});
        }
        shape.tops.push_back(top);
        shape.bottoms.push_back(bottom);
    }
}

void ShapeSurface(const GemShape& shape, float level, const Wave& wave, std::vector<float>& surface)
{
    surface.clear();
    const float line = shape.LevelY(level);
    for (std::size_t i = 0; i < shape.xs.size(); ++i)
    {
        const float x = (shape.xs[i] - shape.box.left) / shape.box.width;
        const float y = line + WaveOffset(x, wave.time, wave.period, level, wave.amplitude);
        surface.push_back(std::clamp(y, shape.tops[i], shape.bottoms[i]));
    }
}

void BuildOrbBody(const GemShape& shape, std::span<const float> surface, float ghostLevel, const BodyShades& shades,
                  std::vector<Vertex>& out)
{
    out.clear();
    if (surface.size() != shape.xs.size())
        return;
    const std::uint32_t empty = Grey(shades.empty);
    const std::uint32_t ghost = Grey(shades.ghost);
    const std::uint32_t fill = Grey(shades.fill);
    const float ghostLine = shape.LevelY(ghostLevel);
    for (std::size_t i = 0; i + 1 < shape.xs.size(); ++i)
    {
        const float x0 = shape.xs[i];
        const float x1 = shape.xs[i + 1];
        const float ghost0 = std::clamp(ghostLine, shape.tops[i], surface[i]);
        const float ghost1 = std::clamp(ghostLine, shape.tops[i + 1], surface[i + 1]);
        PushColumnQuad(shape, x0, shape.tops[i], ghost0, x1, shape.tops[i + 1], ghost1, empty, out);
        PushColumnQuad(shape, x0, ghost0, surface[i], x1, ghost1, surface[i + 1], ghost, out);
        PushColumnQuad(shape, x0, surface[i], shape.bottoms[i], x1, surface[i + 1], shape.bottoms[i + 1], fill, out);
    }
}

void BuildOrbShine(const GemShape& shape, std::span<const float> surface, const ShineInputs& inputs,
                   std::vector<Vertex>& out)
{
    out.clear();
    if (surface.size() != shape.xs.size())
        return;
    PushGlassAndSheen(shape, inputs, out);
    PushCrest(shape, surface, inputs, out);
}

void BuildOrbUnder(const Box& box, std::span<const OutlinePoint> outline, float width, const RimColours& colours,
                   std::vector<Vertex>& out)
{
    out.clear();
    const std::size_t count = outline.size();
    if (count < 3 || width <= 0.f || box.width <= 0.f || box.height <= 0.f)
        return;

    const auto pointAt = [&](std::size_t i)
    { return Point{box.left + outline[i % count].x * box.width, box.top + outline[i % count].y * box.height}; };
    // Clockwise on screen (y down) is a positive shoelace sum; outward is then to the left of each edge.
    float doubledArea = 0.f;
    for (std::size_t i = 0; i < count; ++i)
        doubledArea += pointAt(i).x * pointAt(i + 1).y - pointAt(i + 1).x * pointAt(i).y;
    const float orientation = doubledArea >= 0.f ? 1.f : -1.f;

    // Each corner pushed out along its mitre, so every side of the rim is `width` wide.
    const auto outerAt = [&](std::size_t i)
    {
        const Point here = pointAt(i);
        const Point before = Outward(pointAt(i + count - 1), here, orientation);
        const Point after = Outward(here, pointAt(i + 1), orientation);
        const float mitre = 1.f + before.x * after.x + before.y * after.y;
        if (mitre <= PixelEpsilon)
            return here;
        return Point{here.x + width * (before.x + after.x) / mitre, here.y + width * (before.y + after.y) / mitre};
    };

    for (std::size_t i = 0; i < count; ++i)
    {
        const Point inner0 = pointAt(i);
        const Point inner1 = pointAt(i + 1);
        const Point outer0 = outerAt(i);
        const Point outer1 = outerAt(i + 1);
        out.push_back({outer0.x, outer0.y, 0.f, 0.f, colours.outer});
        out.push_back({inner0.x, inner0.y, 0.f, 0.f, colours.inner});
        out.push_back({inner1.x, inner1.y, 0.f, 0.f, colours.inner});
        out.push_back({outer1.x, outer1.y, 0.f, 0.f, colours.outer});
    }
}

void AppendArcTrack(const Ring& ring, const RimColours& colours, std::vector<Vertex>& out)
{
    const float middle = 0.5f * (ring.inner + ring.outer);
    const float end = ring.startDegrees + ring.sweepDegrees;
    PushArcBand(ring, ring.inner, middle, ring.startDegrees, end, colours.outer, colours.inner, false, out);
    PushArcBand(ring, middle, ring.outer, ring.startDegrees, end, colours.inner, colours.outer, false, out);
}

void BuildArc(const Ring& ring, const ArcInputs& inputs, std::vector<Vertex>& out)
{
    out.clear();
    const ArcRange value = ArcSpan(inputs.level, ring.startDegrees, ring.sweepDegrees);
    const ArcRange ghost = ArcSpan(inputs.ghost, ring.startDegrees, ring.sweepDegrees);
    const std::uint32_t full = Grey(1.f);
    PushArcBand(ring, ring.inner, ring.outer, value.startDegrees, value.endDegrees, full, full, true, out);
    if (ghost.endDegrees > value.endDegrees)
    {
        const std::uint32_t shade = Grey(inputs.ghostShade);
        PushArcBand(ring, ring.inner, ring.outer, value.endDegrees, ghost.endDegrees, shade, shade, true, out);
    }
}

void BuildArcCap(const Ring& ring, float level, float length, float tint, std::vector<Vertex>& out)
{
    out.clear();
    const float middle = 0.5f * (ring.inner + ring.outer);
    if (level <= 0.f || length <= 0.f || tint <= 0.f || middle <= 0.f)
        return;
    const ArcRange value = ArcSpan(level, ring.startDegrees, ring.sweepDegrees);
    const float capDegrees = length / middle / DegreesToRadians;
    const float from = std::max(value.startDegrees, value.endDegrees - capDegrees);
    const std::uint32_t colour = Grey(tint);
    PushArcBand(ring, ring.inner, ring.outer, from, value.endDegrees, colour, colour, true, out);
}

void BuildHalo(const Box& box, std::span<const OutlinePoint> outline, const HaloShape& shape, std::uint32_t colour,
               float strength, std::vector<Vertex>& out)
{
    out.clear();
    if (outline.size() < 3 || strength <= 0.f || shape.radius <= 0.f || box.width <= 0.f || box.height <= 0.f)
        return;

    const Point centre{shape.centreX, shape.centreY};
    std::vector<Point> points;
    std::vector<float> angles;
    points.reserve(outline.size());
    angles.reserve(HaloSegments + outline.size() + 1);
    for (const OutlinePoint& point : outline)
    {
        const Point corner{box.left + point.x * box.width, box.top + point.y * box.height};
        points.push_back(corner);
        float degrees = std::atan2(corner.y - centre.y, corner.x - centre.x) / DegreesToRadians;
        if (degrees < 0.f)
            degrees += FullTurnDegrees;
        angles.push_back(degrees);
    }
    for (int segment = 0; segment < HaloSegments; ++segment)
        angles.push_back(FullTurnDegrees * static_cast<float>(segment) / HaloSegments);
    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end(), [](float a, float b) { return b - a <= AngleEpsilon; }),
                 angles.end());
    if (FullTurnDegrees + angles.front() - angles.back() <= AngleEpsilon)
        angles.pop_back();
    angles.push_back(angles.front() + FullTurnDegrees);

    const std::uint32_t lit = Scaled(colour, strength);
    // The fan's inner edge on the outline and its outer one at the radius, or higher where the group ends.
    const auto edgesAt = [&](float degrees, Point& inner, Point& outer)
    {
        const Point direction = Direction(degrees);
        const float from = ReachInside(centre, direction, points);
        float to = shape.radius;
        if (direction.y > std::numeric_limits<float>::epsilon())
            to = std::min(to, (shape.bottom - centre.y) / direction.y);
        to = std::max(to, from);
        inner = {centre.x + from * direction.x, centre.y + from * direction.y};
        outer = {centre.x + to * direction.x, centre.y + to * direction.y};
    };

    Point inner0;
    Point outer0;
    edgesAt(angles.front(), inner0, outer0);
    for (std::size_t i = 1; i < angles.size(); ++i)
    {
        Point inner1;
        Point outer1;
        edgesAt(angles[i], inner1, outer1);
        const bool open0 = std::hypot(outer0.x - inner0.x, outer0.y - inner0.y) > PixelEpsilon;
        const bool open1 = std::hypot(outer1.x - inner1.x, outer1.y - inner1.y) > PixelEpsilon;
        if (open0 || open1)
        {
            out.push_back({outer0.x, outer0.y, 0.f, 0.f, 0u});
            out.push_back({inner0.x, inner0.y, 0.f, 0.f, lit});
            out.push_back({inner1.x, inner1.y, 0.f, 0.f, lit});
            out.push_back({outer1.x, outer1.y, 0.f, 0.f, 0u});
        }
        inner0 = inner1;
        outer0 = outer1;
    }
}
} // namespace UI::HUD::LiveGauge
