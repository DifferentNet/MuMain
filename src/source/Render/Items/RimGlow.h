#pragma once

#include "Render/Sprites/GlobalBitmap.h"

// The Remaster's enchant glow (+7 and up with HD textures on): instead of the Classic chrome
// layers, which wash the whole piece in light and hide the HD texture under them, the light of
// the item's glow colour runs along the outline of the piece (where the surface turns away from
// the camera) and a bright band sweeps over it now and then. The middle of the piece shows its
// texture. The skinning shader picks the spot of the RimGlow texture for each vertex
// (SkinningTextureCoordinates::Rim): u = how far the surface turns away, v = the band's phase.
namespace Render::Items::RimGlow
{

// True when the Remaster look applies: HD textures on.
[[nodiscard]] bool Enabled();

// The RimGlow texture, made the first time from Effect\Chrome01: u is the outline ramp, v the
// sweeping band (repeats).
[[nodiscard]] GLuint Texture();

// Strength of the outline light for an item level, 0 below +7.
[[nodiscard]] float Strength(int level);

} // namespace Render::Items::RimGlow
