// RibbonSpline.h: smooth skill trails (Remaster, Render/Effects/SkillFx.h, Feature::Ribbons).
//
// A joint trail keeps one tail (four corners) per 25 FPS reference frame and the newest tail follows
// the joint every rendered frame. Drawn as is, the trail is a polyline with a kink at every tail, its
// texture slides one tail at a time and its end drops a whole span at once. Here the tails are joined
// by a centripetal Catmull-Rom curve (it never loops or overshoots between close tails) cut into a
// few pieces where the trail turns, the texture U follows each tail's age and the end is cut at the
// trail's length, so U and the end move every frame. At 25 FPS with a full trail and one piece per
// span, the points and U are exactly the tails and U of the classic trail.
#pragma once

#include <algorithm>
#include <cmath>

namespace Render::Effects::RibbonSpline
{

// Chords shorter than this (in world units) count as no chord: the curve does not divide by them.
inline constexpr float kMinChord = 0.01f;
// One piece per this many degrees the trail turns over a span.
inline constexpr float kDegreesPerPiece = 5.0f;
// Longer and shorter trails are drawn as classic.
inline constexpr int kMaxRibbonTails = 32;
inline constexpr int kMinRibbonTails = 3;

inline constexpr int kCorners = 4;
inline constexpr int kAxes = 3;

using Tail = float[kCorners][kAxes];

// One point across the trail: its four corners and its texture U.
struct RibbonPoint
{
    float corner[kCorners][kAxes];
    float u;
};

[[nodiscard]] inline float Distance(const float* a, const float* b)
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The centripetal (Barry-Goldman) Catmull-Rom point between p1 (s = 0) and p2 (s = 1), from the chord
// lengths d01, d12 and d23 (each at least kMinChord). The ends are returned exactly.
inline void CatmullRom(const float* p0, const float* p1, const float* p2, const float* p3, const float d01,
                       const float d12, const float d23, const float s, float* out)
{
    if (s <= 0.0f)
    {
        std::copy(p1, p1 + kAxes, out);
        return;
    }
    if (s >= 1.0f)
    {
        std::copy(p2, p2 + kAxes, out);
        return;
    }
    const float t1 = std::sqrt(d01);
    const float t2 = t1 + std::sqrt(d12);
    const float t3 = t2 + std::sqrt(d23);
    const float t = t1 + s * (t2 - t1);
    for (int axis = 0; axis < kAxes; ++axis)
    {
        const float a1 = ((t1 - t) * p0[axis] + t * p1[axis]) / t1;
        const float a2 = ((t2 - t) * p1[axis] + (t - t1) * p2[axis]) / (t2 - t1);
        const float a3 = ((t3 - t) * p2[axis] + (t - t2) * p3[axis]) / (t3 - t2);
        const float b1 = ((t2 - t) * a1 + t * a2) / t2;
        const float b2 = ((t3 - t) * a2 + (t - t1) * a3) / (t3 - t1);
        out[axis] = ((t2 - t) * b1 + (t - t1) * b2) / (t2 - t1);
    }
}

// The pieces a span takes for a turn of `turnDegrees`: one per kDegreesPerPiece, 1 to maxPieces.
[[nodiscard]] inline int Pieces(const float turnDegrees, const int maxPieces)
{
    const int wanted = static_cast<int>(std::ceil(turnDegrees / kDegreesPerPiece));
    return std::clamp(wanted, 1, std::max(maxPieces, 1));
}

// The most pieces a span of one trail takes: `kept` stays with the trail from frame to frame, `built`
// is this frame's (kept, or 1 past the hard cap).
struct StickyChoice
{
    int kept;
    int built;
};

// A trail chooses its most pieces a span when it starts to be drawn (no kept choice, or not drawn the
// frame before): maxPieces when even its longest trail (`worstQuads`, quads at maxPieces) fits in what
// the frame's `budget` has left after `quadsSoFar`, else 1, as many quads as the classic trail. It then
// keeps that choice (no more than maxPieces) while it is drawn every frame, so it does not switch its
// look near the budget; a trail that was not drawn a frame (out of view) chooses again. Past twice the
// budget, counting the trail's own longest at its choice, it is built with 1 piece that frame.
// worstQuads / most * kept is exact while worstQuads is a multiple of maxPieces, as JointRibbon::Append
// passes it (spans x maxPieces x faces).
[[nodiscard]] constexpr StickyChoice StickyPieces(const int previous, const bool drawnLastFrame, const int maxPieces,
                                                  const int quadsSoFar, const int worstQuads, const int budget)
{
    const int most = std::max(maxPieces, 1);
    int kept = 0;
    if (previous > 0 && drawnLastFrame)
    {
        kept = std::min(previous, most);
    }
    else
    {
        kept = quadsSoFar + worstQuads <= budget ? most : 1;
    }
    const int worstAtKept = worstQuads / most * kept;
    const int built = quadsSoFar + worstAtKept > 2 * budget ? 1 : kept;
    return StickyChoice{kept, built};
}

// A tail's age in reference frames. Slot 0 is the joint now; slot 1 was laid `phase` ago (the joint's
// TailFrames plus this frame's FPS_ANIMATION_FACTOR, 1 at 25 FPS), each older slot one more.
[[nodiscard]] constexpr float TailAge(const int slot, const float phase)
{
    return slot == 0 ? 0.0f : static_cast<float>(slot - 1) + phase;
}

// The trail length in tail ages: the classic full trail, maxTails - 2 + frame factor, or less while it grows.
[[nodiscard]] constexpr float FullLength(const int maxTails, const float frameFactor)
{
    return static_cast<float>(maxTails - 2) + frameFactor;
}

// The age where the trail ends: the oldest tail, cut at the full trail length.
[[nodiscard]] constexpr float EndAge(const int lastValidSlot, const float phase, const int maxTails,
                                     const float frameFactor)
{
    return std::min(TailAge(lastValidSlot, phase), FullLength(maxTails, frameFactor));
}

// The texture U at an age: 0 at the trail's end, 1 a full trail length before it. At 25 FPS with a full
// trail it is the classic (NumTails - j) / (MaxTails - 1).
[[nodiscard]] constexpr float TrailU(const float age, const float endAge, const int maxTails, const float frameFactor)
{
    return (endAge - age) / FullLength(maxTails, frameFactor);
}

namespace Detail
{
inline void Centre(const Tail& tail, float* out)
{
    for (int axis = 0; axis < kAxes; ++axis)
    {
        out[axis] = (tail[0][axis] + tail[1][axis]) * 0.5f;
    }
}

// The angle in degrees between the chords a->b and b->c; 0 when either is shorter than kMinChord.
inline float TurnDegrees(const float* a, const float* b, const float* c)
{
    float ab[kAxes];
    float bc[kAxes];
    for (int axis = 0; axis < kAxes; ++axis)
    {
        ab[axis] = b[axis] - a[axis];
        bc[axis] = c[axis] - b[axis];
    }
    const float lengths = Distance(a, b) * Distance(b, c);
    if (lengths < kMinChord * kMinChord)
    {
        return 0.0f;
    }
    const float cosine = std::clamp((ab[0] * bc[0] + ab[1] * bc[1] + ab[2] * bc[2]) / lengths, -1.0f, 1.0f);
    constexpr float kDegreesPerRadian = 57.2957795f;
    return std::acos(cosine) * kDegreesPerRadian;
}

// The trail's centres, one per tail.
struct Centres
{
    float point[kMaxRibbonTails][kAxes];
    int count;
};

// One span between tails j and j + 1, with the neighbours the curve leans on and their chords.
struct Span
{
    const Tail* tails;
    const Centres* centres;
    int j;
    bool linear;
    bool phantomStart;
    bool phantomEnd;
    float d01;
    float d12;
    float d23;
};

inline Span MakeSpan(const Tail* tails, const Centres& centres, const int j)
{
    Span span{tails, &centres, j, false, false, false, 0.0f, 0.0f, 0.0f};
    const float* c1 = centres.point[j];
    const float* c2 = centres.point[j + 1];
    span.d12 = Distance(c1, c2);
    if (span.d12 < kMinChord)
    {
        span.linear = true;
        return span;
    }
    span.d01 = j > 0 ? Distance(centres.point[j - 1], c1) : 0.0f;
    if (span.d01 < kMinChord)
    {
        span.phantomStart = true;
        span.d01 = span.d12;
    }
    span.d23 = j + 2 < centres.count ? Distance(c2, centres.point[j + 2]) : 0.0f;
    if (span.d23 < kMinChord)
    {
        span.phantomEnd = true;
        span.d23 = span.d12;
    }
    return span;
}

inline float SpanTurn(const Span& span)
{
    if (span.linear)
    {
        return 0.0f;
    }
    const Centres& c = *span.centres;
    const int j = span.j;
    const float start = span.phantomStart ? 0.0f : TurnDegrees(c.point[j - 1], c.point[j], c.point[j + 1]);
    const float end = span.phantomEnd ? 0.0f : TurnDegrees(c.point[j], c.point[j + 1], c.point[j + 2]);
    return std::max(start, end);
}

// One corner of the span at s (0..1).
inline void SpanCorner(const Span& span, const int corner, const float s, float* out)
{
    const float* p1 = span.tails[span.j][corner];
    const float* p2 = span.tails[span.j + 1][corner];
    if (span.linear)
    {
        if (s <= 0.0f || s >= 1.0f)
        {
            const float* end = s <= 0.0f ? p1 : p2;
            std::copy(end, end + kAxes, out);
            return;
        }
        for (int axis = 0; axis < kAxes; ++axis)
        {
            out[axis] = p1[axis] + (p2[axis] - p1[axis]) * s;
        }
        return;
    }
    float phantom0[kAxes];
    float phantom3[kAxes];
    for (int axis = 0; axis < kAxes; ++axis)
    {
        phantom0[axis] = 2.0f * p1[axis] - p2[axis];
        phantom3[axis] = 2.0f * p2[axis] - p1[axis];
    }
    const float* p0 = span.phantomStart ? phantom0 : span.tails[span.j - 1][corner];
    const float* p3 = span.phantomEnd ? phantom3 : span.tails[span.j + 2][corner];
    CatmullRom(p0, p1, p2, p3, span.d01, span.d12, span.d23, s, out);
}
} // namespace Detail

// Lays the trail's points into `out` (at most `cap`) and returns how many it laid: the first
// `validCount` tails (2 to kMaxRibbonTails), each span cut into Pieces(turn, maxPieces) pieces and the
// trail ended at EndAge. Every corner follows the curve through its own tails, with the chords of the
// tails' centres (corners 0 and 1's midpoint) as the knots, so the corners stay in step.
[[nodiscard]] inline int Build(const Tail* tails, const int validCount, const int maxTails, const float phase,
                               const float frameFactor, const int maxPieces, RibbonPoint* out, const int cap)
{
    if (validCount < 2 || validCount > kMaxRibbonTails || cap <= 0)
    {
        return 0;
    }
    Detail::Centres centres{};
    centres.count = validCount;
    for (int j = 0; j < validCount; ++j)
    {
        Detail::Centre(tails[j], centres.point[j]);
    }
    const float endAge = EndAge(validCount - 1, phase, maxTails, frameFactor);
    const auto emit = [&](const Detail::Span& span, const float s, const float age, RibbonPoint& point)
    {
        for (int corner = 0; corner < kCorners; ++corner)
        {
            Detail::SpanCorner(span, corner, s, point.corner[corner]);
        }
        point.u = TrailU(age, endAge, maxTails, frameFactor);
    };

    int count = 0;
    emit(Detail::MakeSpan(tails, centres, 0), 0.0f, 0.0f, out[count++]);
    for (int j = 0; j + 1 < validCount && count < cap; ++j)
    {
        const float startAge = TailAge(j, phase);
        const float endOfSpan = TailAge(j + 1, phase);
        if (startAge >= endAge)
        {
            break;
        }
        const float spanAges = endOfSpan - startAge;
        // The share of the span the trail still covers: the last span is cut at the trail's end.
        const float reach = (endOfSpan > endAge && spanAges > 0.0f) ? (endAge - startAge) / spanAges : 1.0f;
        const Detail::Span span = Detail::MakeSpan(tails, centres, j);
        const int pieces = Pieces(Detail::SpanTurn(span), maxPieces);
        for (int piece = 1; piece <= pieces && count < cap; ++piece)
        {
            const float s = reach * static_cast<float>(piece) / static_cast<float>(pieces);
            emit(span, s, startAge + spanAges * s, out[count++]);
        }
    }
    return count;
}

} // namespace Render::Effects::RibbonSpline
