// WheelRibbon.h: Twisting Slash's arc band (Remaster smooth trails, Render/Effects/SkillFx.h,
// Feature::Ribbons).
//
// Twisting Slash draws no trail of its own: ghost weapons (MODEL_SKILL_WHEEL2, one laid per 25 FPS frame,
// each next one 18 degrees behind the one before and fainter) circle the caster, moved every rendered frame by
// Move_MODEL_SKILL_WHEEL2 to the caster's position plus (0, -150, 0) (spears -180) turned by the
// ghost's angle, which then steps back 18 degrees per 25 FPS frame. The orbit is known exactly, so
// with smooth trails on the lead ghost lays a flat band over the arc it has swept: in the orbit's
// plane at the ghost weapon's height, from the lead weapon back up to kArcDegrees, its texture U and
// light running from the weapon to the band's end, fading out over the ghost's last kFadeTicks.
// The band keeps no state and creates nothing; the ghost weapons are drawn as before on top of it.
//
// The lead ghost is the first one its cast (MODEL_SKILL_WHEEL1) lays. Neither SubType (4 - the cast's
// LifeTime, -1 for the first ghost) nor the ghost count tells it: above 25 FPS a ghost is laid with
// FPS_ANIMATION_FACTOR odds each frame, so a cast lays 0 to several ghosts of each SubType. So the cast
// marks its first ghost in m_bySkillSerialNum (LayGhost). For MODEL_SKILL_WHEEL1 and WHEEL2 nothing else
// reads that field: no skill sends or cancels with these effects' serial, CreateEffect writes the value
// it is passed (0 by default), and their move and render code never look at it. So the mark is written
// whether or not the band shows and never changes the classic draw.
#pragma once

#include "Render/Effects/SkillFx.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

class OBJECT;

namespace Render::Effects::WheelRibbon
{

// Move_MODEL_SKILL_WHEEL2: the ghost's angle steps back this far per 25 FPS frame.
inline constexpr float kDegreesPerTick = 18.0f;
// CreateEffect: a ghost weapon's life in 25 FPS frames.
inline constexpr float kLifeTicks = 25.0f;
// The most arc the band covers behind the lead weapon.
inline constexpr float kArcDegrees = 90.0f;
// The band reaches this far inside and outside the orbit.
inline constexpr float kHalfWidth = 45.0f;
// Move_MODEL_SKILL_WHEEL2: the orbit's radius, with a sword and with a spear.
inline constexpr float kSwordRadius = 150.0f;
inline constexpr float kSpearRadius = 180.0f;
// RenderWheelWeapon draws the weapon this far above its position.
inline constexpr float kHeight = 100.0f;
// The band fades out over the ghost's last 25 FPS frames.
inline constexpr float kFadeTicks = 5.0f;
// One span of the arc, cut into the trail pieces of the effect level (SkillFx::RibbonPieceBudget).
inline constexpr float kDegreesPerSpan = 40.0f;
// The most pieces a span takes (effect level 4) and so the most quads a band has.
inline constexpr int kMaxPiecesPerSpan = SkillFx::RibbonPieceBudget(4);
inline constexpr int kMaxPieces = 18;
static_assert(kMaxPieces * kDegreesPerSpan >= kArcDegrees * kMaxPiecesPerSpan, "kMaxPieces must hold the finest arc");
// The sword skill orange the band is lit with.
inline constexpr float kColor[3] = {1.0f, 0.6f, 0.2f};

// m_bySkillSerialNum of a cast (MODEL_SKILL_WHEEL1): no ghost laid yet (CreateEffect's default), or some.
inline constexpr std::uint8_t kNoGhostLaid = 0;
inline constexpr std::uint8_t kGhostsLaid = 1;
// m_bySkillSerialNum of a ghost (MODEL_SKILL_WHEEL2): the lead, or one that follows it.
inline constexpr std::uint8_t kFollowerGhost = 0;
inline constexpr std::uint8_t kLeadGhost = 1;

// A ghost laid by a cast: the cast's mark after it and the ghost's own mark.
struct LaidGhost
{
    std::uint8_t castMark;
    std::uint8_t ghostMark;
};

// Lays a ghost for a cast whose mark is `castMark`: the first ghost of a cast is the lead.
[[nodiscard]] constexpr LaidGhost LayGhost(const std::uint8_t castMark)
{
    return LaidGhost{kGhostsLaid, castMark == kNoGhostLaid ? kLeadGhost : kFollowerGhost};
}

// Whether a ghost with this mark is its cast's lead, the one that draws the band.
[[nodiscard]] constexpr bool IsLeadGhost(const std::uint8_t ghostMark)
{
    return ghostMark == kLeadGhost;
}

// The angle (degrees) the lead weapon is drawn at: the move handler placed it before stepping the
// ghost's angle by this frame's share of a tick.
[[nodiscard]] constexpr float HeadDegrees(const float angleZ, const float frameFactor)
{
    return angleZ + kDegreesPerTick * frameFactor;
}

// The arc (degrees) the band covers behind the drawn weapon: the arc the weapon swept since its first
// move, up to kArcDegrees.
[[nodiscard]] constexpr float Arc(const float lifeTime, const float frameFactor)
{
    const float swept = kDegreesPerTick * (kLifeTicks - lifeTime - frameFactor);
    return std::clamp(swept, 0.0f, kArcDegrees);
}

// The pieces the band is cut into: one per kDegreesPerSpan / piecesPerSpan degrees of arc, none when
// the arc is empty or the trails are off (piecesPerSpan 0).
[[nodiscard]] inline int Pieces(const float arcDegrees, const int piecesPerSpan)
{
    if (piecesPerSpan <= 0 || arcDegrees <= 0.0f)
    {
        return 0;
    }
    const float degreesPerPiece = kDegreesPerSpan / static_cast<float>(std::min(piecesPerSpan, kMaxPiecesPerSpan));
    const int wanted = static_cast<int>(std::ceil(arcDegrees / degreesPerPiece));
    return std::clamp(wanted, 1, kMaxPieces);
}

// The band's light over the ghost's last kFadeTicks: 1 before, 0 at its end.
[[nodiscard]] constexpr float EndFade(const float lifeTime)
{
    return std::clamp(lifeTime / kFadeTicks, 0.0f, 1.0f);
}

// The light at a share `along` (0 at the weapon, 1 at the band's end) of the band.
[[nodiscard]] constexpr float Light(const float along, const float fade)
{
    return (1.0f - along) * fade;
}

// The orbit radius for the caster's weapon.
[[nodiscard]] constexpr float Radius(const bool spear)
{
    return spear ? kSpearRadius : kSwordRadius;
}

// A band point: `centre` plus (0, -radius, 0) turned by `matrix` (AngleMatrix of the point's angles, as
// VectorRotate turns it in the move handler), lifted by kHeight.
inline void Point(const float matrix[3][4], const float centre[3], const float radius, float out[3])
{
    for (int axis = 0; axis < 3; ++axis)
    {
        out[axis] = centre[axis] - radius * matrix[axis][1];
    }
    out[2] += kHeight;
}

// Draws the lead ghost's band (IsLeadGhost, with an owner) with `piecesPerSpan` pieces per kDegreesPerSpan
// (SkillFx::ActiveRibbonPieces). Main thread only (RenderEffects).
void Draw(const OBJECT& ghost, int piecesPerSpan);

// The bands drawn in the last whole frame ($fx status).
[[nodiscard]] int LastFrameArcs();

} // namespace Render::Effects::WheelRibbon
