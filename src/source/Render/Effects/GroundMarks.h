// GroundMarks.h: fading marks that fire, ice and earth skills leave on the ground (Remaster,
// Render/Effects/SkillFx.h, Feature::Decals).
//
//   Scorch  Meteor, Flame, Rageful Blow and Earthshake: the crater texture (Effect/Cratered), alpha
//           blended, white (the earth skills keep their crack model on top: its white strokes, flat on
//           the ground, read as noise).
//   Large   Inferno and Hellfire: one wide scorch, a little fainter.
//   Frost   Ice Storm and Ice: the soft cloud texture (Effect/Clud64) added in a pale blue.
//
// A mark fades in over 2 reference frames (25 Hz ticks), holds, then fades out as 1 - t^2. The marks
// lie in a fixed pool (SkillFx::MarkBudget of the effect level, at most 48); a hit close to a mark of
// the same kind renews that mark instead of laying another, and a full pool gives up the mark furthest
// through its life. Water maps (Kalima), Atlans, Icarus and Kanturu's Maya scene (no ground drawn) have
// none. A mark follows the ground: each terrain tile under it gets a piece
// clipped to the mark, 5 units above the ground. No lights, no new textures, no shader.
//
// The pure parts (fade, pool, tiles, mirror) are here for the tests; the client parts are below them.
// Main thread only.
#pragma once

#include "Render/Effects/SkillFx.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace Render::Effects::GroundMarks
{

enum class Kind : std::uint8_t
{
    Scorch,
    LargeScorch,
    Frost,
};
inline constexpr int kKindCount = 3;

// The skills that lay a mark.
enum class Source : std::uint8_t
{
    Meteor,
    Flame,
    Inferno,
    Hellfire,
    IceStorm,
    Ice,
    RagefulBlow, // and Earthshake: a scorch under their crack model
};

// How long a kind of mark lasts, in reference frames (25 Hz), and how strong it is at its peak.
struct Life
{
    float hold; // from laying to the start of the fade out (the fade in is the first kFadeInTicks)
    float fade; // the fade out
    float peak; // the alpha (Frost: the brightness) while it holds
};

inline constexpr float kFadeInTicks = 2.f;

[[nodiscard]] constexpr Life LifeOf(const Kind kind)
{
    switch (kind)
    {
    // The crater art is mostly soft edge (its alpha averages a third), and the Remaster's grass stands
    // over the ground: a scorch holds at full alpha to be seen at night.
    case Kind::Scorch:
        return {25.f, 100.f, 1.f};
    case Kind::LargeScorch:
        return {25.f, 100.f, 0.85f};
    case Kind::Frost:
        return {15.f, 60.f, 0.5f};
    }
    return {0.f, 0.f, 0.f};
}

// The age at which a mark is gone.
[[nodiscard]] constexpr float Lifetime(const Kind kind)
{
    const Life life = LifeOf(kind);
    return life.hold + life.fade;
}

// The alpha of a mark at `age` reference frames: 0 at laying, the peak after the fade in, 0 at Lifetime.
[[nodiscard]] constexpr float Alpha(const float age, const Kind kind)
{
    const Life life = LifeOf(kind);
    if (!(age > 0.f) || age >= life.hold + life.fade)
    {
        return 0.f;
    }
    const float fadeIn = age < kFadeInTicks ? age / kFadeInTicks : 1.f;
    if (age <= life.hold)
    {
        return life.peak * fadeIn;
    }
    const float t = (age - life.hold) / life.fade;
    return life.peak * fadeIn * (1.f - t * t);
}

// The terrain tile size (TERRAIN_SCALE) and the widest mark: a mark covers at most 4 x 4 tiles.
inline constexpr float kTileSize = 100.f;
inline constexpr int kMaxTilesAcross = 4;
inline constexpr float kMaxHalfSize = 150.f;
static_assert(static_cast<int>(2.f * kMaxHalfSize / kTileSize) + 1 <= kMaxTilesAcross,
              "the widest mark must fit in kMaxTilesAcross tiles");

struct Look
{
    Kind kind;
    float halfW; // half the mark along x (the texture's U), in world units
    float halfH; // half the mark along y (V)
};

// The mark a skill lays.
[[nodiscard]] constexpr Look LookOf(const Source source)
{
    switch (source)
    {
    case Source::Meteor:
        return {Kind::Scorch, 90.f, 90.f};
    case Source::Flame:
        return {Kind::Scorch, 80.f, 80.f};
    case Source::Inferno:
    case Source::Hellfire:
        return {Kind::LargeScorch, kMaxHalfSize, kMaxHalfSize};
    case Source::IceStorm: // one mark a cast on the targeted spot, over the whole storm
        return {Kind::Frost, kMaxHalfSize, kMaxHalfSize};
    case Source::Ice:
        return {Kind::Frost, 70.f, 70.f};
    case Source::RagefulBlow:
        return {Kind::Scorch, 75.f, 75.f};
    }
    return {Kind::Scorch, 0.f, 0.f};
}

struct Mark
{
    float x = 0.f;
    float y = 0.f;
    float halfW = 0.f;
    float halfH = 0.f;
    float age = 0.f;
    Kind kind = Kind::Scorch;
    std::uint8_t mirror = 0; // MirrorOf the position
};

// A hit renews a mark of the same kind whose centre is closer than this share of the larger half size:
// scorches the whole size, so overlapping scorches never stack up to a near black patch.
[[nodiscard]] constexpr float MergeShareOf(const Kind kind)
{
    return kind == Kind::Scorch || kind == Kind::LargeScorch ? 1.f : 0.6f;
}
inline constexpr int kPoolSize = SkillFx::kMaxMarks;

// The marks that lie now, oldest laid first (a renewed mark keeps its place).
class MarkPool
{
public:
    // Lays `mark` (its age is taken as 0) or renews a close mark of the same kind; past `budget` marks
    // the mark furthest through its life goes first. False when the budget is 0.
    bool Add(Mark mark, int budget)
    {
        budget = std::min(budget, kPoolSize);
        if (budget <= 0)
        {
            return false;
        }
        mark.halfW = std::clamp(mark.halfW, 0.f, kMaxHalfSize);
        mark.halfH = std::clamp(mark.halfH, 0.f, kMaxHalfSize);
        mark.age = 0.f;
        for (int i = 0; i < m_count; ++i)
        {
            Mark& old = m_marks[i];
            const float reach = MergeShareOf(mark.kind) *
                                std::max(std::max(old.halfW, old.halfH), std::max(mark.halfW, mark.halfH));
            const float dx = old.x - mark.x;
            const float dy = old.y - mark.y;
            if (old.kind == mark.kind && dx * dx + dy * dy < reach * reach)
            {
                // Back to the start of its hold; a mark still fading in goes on fading in.
                old.age = std::min(old.age, kFadeInTicks);
                old.halfW = std::max(old.halfW, mark.halfW);
                old.halfH = std::max(old.halfH, mark.halfH);
                return true;
            }
        }
        while (m_count >= budget)
        {
            RemoveAt(Oldest());
        }
        m_marks[m_count++] = mark;
        return true;
    }

    // Ages the marks by `ticks` reference frames, drops the faded ones and, past `budget`, those furthest
    // through their life.
    void Update(const float ticks, int budget)
    {
        budget = std::min(budget, kPoolSize);
        if (budget <= 0)
        {
            Clear();
            return;
        }
        int kept = 0;
        for (int i = 0; i < m_count; ++i)
        {
            Mark mark = m_marks[i];
            mark.age += ticks;
            if (mark.age < Lifetime(mark.kind))
            {
                m_marks[kept++] = mark;
            }
        }
        m_count = kept;
        while (m_count > budget)
        {
            RemoveAt(Oldest());
        }
    }

    void Clear() { m_count = 0; }
    [[nodiscard]] int Count() const { return m_count; }
    [[nodiscard]] const Mark& At(const int index) const { return m_marks[index]; }

private:
    // The mark furthest through its life (age / Lifetime): a short frost goes before a young scorch.
    [[nodiscard]] int Oldest() const
    {
        int oldest = 0;
        for (int i = 1; i < m_count; ++i)
        {
            if (LifeShare(m_marks[i]) > LifeShare(m_marks[oldest]))
            {
                oldest = i;
            }
        }
        return oldest;
    }

    [[nodiscard]] static float LifeShare(const Mark& mark) { return mark.age / Lifetime(mark.kind); }

    // Keeps the order of the others, so overlapping marks never swap their draw order.
    void RemoveAt(const int index)
    {
        for (int i = index; i + 1 < m_count; ++i)
        {
            m_marks[i] = m_marks[i + 1];
        }
        --m_count;
    }

    std::array<Mark, kPoolSize> m_marks{};
    int m_count = 0;
};

[[nodiscard]] constexpr int FloorToInt(const float value)
{
    const int truncated = static_cast<int>(value);
    return static_cast<float>(truncated) > value ? truncated - 1 : truncated;
}

// The tiles a mark covers along one axis, first to last (both included).
struct TileSpan
{
    int first;
    int last;
};

[[nodiscard]] constexpr TileSpan TilesUnder(const float center, const float half, const float tileSize = kTileSize)
{
    const int first = FloorToInt((center - half) / tileSize);
    const float end = (center + half) / tileSize;
    int last = FloorToInt(end);
    // An edge right on a tile border does not reach into the next tile.
    if (static_cast<float>(last) == end && last > first)
    {
        --last;
    }
    return {first, std::min(last, first + kMaxTilesAcross - 1)};
}

// The part of a mark's span [center - half, center + half] inside `tile`; empty when hi <= lo.
struct Span
{
    float lo;
    float hi;
};

[[nodiscard]] constexpr Span ClipToTile(const float center, const float half, const int tile,
                                        const float tileSize = kTileSize)
{
    const float tileLo = static_cast<float>(tile) * tileSize;
    return {std::max(center - half, tileLo), std::min(center + half, tileLo + tileSize)};
}

// One of 8 mirror variants (bit 0: U flipped, bit 1: V flipped, bit 2: U and V swapped), the same for
// the same spot every time (no random numbers).
[[nodiscard]] constexpr std::uint8_t MirrorOf(const float x, const float y)
{
    auto hash = static_cast<std::uint32_t>(static_cast<std::int32_t>(x)) * 0x9E3779B1u;
    hash ^= static_cast<std::uint32_t>(static_cast<std::int32_t>(y)) * 0x85EBCA77u;
    hash ^= hash >> 15;
    hash *= 0x2C1B3C6Du;
    hash ^= hash >> 12;
    return static_cast<std::uint8_t>(hash & 7u);
}

// The mirror variant of a mark: a mark that is not square never swaps U and V, so its art is never
// squashed.
[[nodiscard]] constexpr std::uint8_t MirrorFor(const float x, const float y, const float halfW, const float halfH)
{
    const std::uint8_t mirror = MirrorOf(x, y);
    return halfW == halfH ? mirror : static_cast<std::uint8_t>(mirror & 3u);
}

struct UV
{
    float u;
    float v;
};

// The texture coordinate of a point at share (u, v) of the mark (0..1 each), mirrored.
[[nodiscard]] constexpr UV MirroredUV(float u, float v, const std::uint8_t mirror)
{
    if ((mirror & 4u) != 0)
    {
        const float swapped = u;
        u = v;
        v = swapped;
    }
    if ((mirror & 1u) != 0)
    {
        u = 1.f - u;
    }
    if ((mirror & 2u) != 0)
    {
        v = 1.f - v;
    }
    return {u, v};
}

// The client: lays a skill's mark at (x, y) when Decals is on, the map draws its ground and is neither
// a water map nor Atlans, the mark's texture is loaded and the ground there is ground. True when a mark was laid or renewed.
bool Add(Source source, float x, float y);
// Ages the marks (beside MoveEffects); clears them on a new map and while Decals is off.
void Update();
// Draws the marks (effects pass, before RenderEffects): depth test on, no depth writes.
void Draw();
// Drops all marks.
void Clear();
// The marks lying now ($fx status).
[[nodiscard]] int Count();

} // namespace Render::Effects::GroundMarks
