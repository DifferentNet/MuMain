// FlipbookBlend.h: explosion and fire sheets that blend from one frame to the next (Remaster,
// Render/Effects/SkillFx.h, Feature::FlipbookBlend).
//
// A sheet particle's frame comes from its remaining life, e.g. (20 - LifeTime) / 2, cut to a whole
// frame. ZzzEffectParticle keeps the uncut value, the phase, beside the particle; an additive sheet then
// draws its frame and the next one, each with its share of the light, so the sum stays as bright as
// one frame while the picture moves smoothly. The last frame holds: it never blends towards a frame
// past the sheet's end.
#pragma once

namespace Render::Effects::FlipbookBlend
{

// No phase recorded for the particle's current life: it draws its frame alone.
inline constexpr float kNoPhase = -1.0f;

// Frames on the explosion sheets Effect/Explotion01 and Effect/explotion01mono (BITMAP_EXPLOTION,
// BITMAP_EXPLOTION_MONO). They are 4x4 grids, but only cells 0..9 hold the fire; cells 10..15 are
// solid white. Their phase, (20 - LifeTime) / 2, stays below 10, so the cut frame never reaches a
// white cell; the blend must hold frame 9 too, or it lays a white square over the fading fire.
inline constexpr int kExplosionSheetFrames = 10;

// Frames on Effect/loungexflow (BITMAP_SUMMON_SAHAMUTT_EXPLOSION): a full 4x4 grid.
inline constexpr int kSahamuttExplosionSheetFrames = 16;

struct Frames
{
    int frame = 0;      // the whole frame, as the particle's Frame
    int next = 0;       // the frame it blends towards (== frame when it does not blend)
    float weight = 0.f; // the next frame's share of the light, 0..1
};

// The frames a sheet of `count` frames shows at `phase`. A phase that is not a number or below 0
// shows frame 0, one at or past the last frame holds it; neither blends. The phase is cut to a
// frame only once it is known to lie within the sheet.
[[nodiscard]] constexpr Frames Split(const float phase, const int count)
{
    if (!(phase >= 0.0f))
    {
        return {0, 0, 0.0f};
    }
    const int last = count > 1 ? count - 1 : 0;
    if (phase >= static_cast<float>(last))
    {
        return {last, last, 0.0f};
    }
    const int frame = static_cast<int>(phase);
    return {frame, frame + 1, phase - static_cast<float>(frame)};
}

} // namespace Render::Effects::FlipbookBlend
