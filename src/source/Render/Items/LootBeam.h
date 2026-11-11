#pragma once

// The loot filter's beam: a short vertical light over each highlighted item on the ground, tinted by rarity
// (ghinzuro 2026-11-11: «Луч вверх», excellent green, ancient blue, jewels yellow, the rest warm white).
// Drawn as camera-facing quads (a soft glow and a bright core) with the shared light sprite (BITMAP_LIGHT) through
// RenderQuad3D, the same path as RenderSprite, so it looks the same on Classic, Remaster and /play (WebGPU).
namespace Render::Items::LootBeam
{
// Draws the beams of all visible highlighted ground items; call inside RenderItems after the items are drawn.
void RenderAll();
} // namespace Render::Items::LootBeam
