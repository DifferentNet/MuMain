#pragma once

#include <string_view>

// Strewn leaf litter: map objects whose every mesh wears tree_08 (Lorencia's Grass models, the
// Arena's Object32/33). The original maps hang many of these sprigs up to 4 m above the ground,
// draped over the fountain, walls and pillars. Classic's dim picture hides that; with the
// Remaster's HD textures they read as leaves floating in the air (ghinzuro 2026-11-11), so the
// Remaster leaves the hanging ones out and lets none of them cast the sun shadow.
namespace Render::Models::LeafLitter
{
// How far above the ground (world units, 100 per tile) a sprig may lie and still be drawn.
inline constexpr float kMaxHeightAboveGround = 40.0f;

// Whether a model texture name from a BMD ("tree_08.tga", or a path ending in one) is the leaf
// litter picture. Either separator and any case.
[[nodiscard]] bool IsLeafLitterTexture(std::string_view textureName);

// Whether a sprig at height `objectZ` over ground at `groundZ` hangs in the air.
[[nodiscard]] bool Floats(float objectZ, float groundZ);
} // namespace Render::Models::LeafLitter
