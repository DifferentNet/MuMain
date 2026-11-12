// TerrainVariation.h: breaks up the repeat of the ground tile textures in the Remaster
// (desktop, HD textures on), from the player's [Render] TerrainVariation settings
// (ghinzuro 2026-11-12, Remaster plan step 5).
//
//   Macro colour: the ground's vertex light is multiplied per terrain vertex by two layers of
//   smooth value noise, one for brightness and one for a warm/cool shift, with patches of about
//   kMacroCellTiles tiles; neighbouring texture repeats then never look identical.
//   Flips: each texture repeat (FaceTexture: a 256-wide texture covers 4x4 tiles) is mirrored in u,
//   v, both or neither, picked by a hash of its block. Mirroring keeps every block edge seamless:
//   the texture is tileable, and a mirrored block shows the same edge texels its neighbour ends
//   with. (A quarter turn would not: it puts a different edge next to the neighbour.)
#pragma once

namespace Render::Terrain::Variation
{

// [Render] TerrainVariation values.
enum class Variant
{
    Off = 0,
    Macro = 1,         // macro colour only
    MacroAndFlips = 2, // macro colour and mirrored blocks
};

inline constexpr float kPercentPerUnit = 100.0f;

// Patch sizes of the two noise octaves, in tiles (a tile is 100 world units), and their weights.
inline constexpr float kMacroCellTiles = 11.0f;
inline constexpr float kMacroDetailCellTiles = 4.3f;
inline constexpr float kMacroWeight = 0.7f;
inline constexpr float kMacroDetailWeight = 0.3f;
// At full strength: brightness within 1 -+ kBrightnessSwing, red up and blue down (warm) or the
// reverse (cool) by up to kWarmthSwing.
inline constexpr float kBrightnessSwing = 0.12f;
inline constexpr float kWarmthSwing = 0.05f;

struct Params
{
    bool macro = false;
    bool flips = false;
    float strength = 0.0f; // 0..1, scales the macro swings (flips are all or nothing)
};

// The params for the saved settings: off when not available (no HD textures, the browser build),
// for variant 0 or an unknown one, and at strength 0. strengthPercent is clamped to 0..100.
[[nodiscard]] Params Resolve(int variant, int strengthPercent, bool available);

// The factor (r, g, b) a ground vertex's light is multiplied by at terrain vertex (x, y); seed
// changes the pattern (the map number). All 1 at strength 0.
void MacroFactor(int x, int y, int seed, float strength, float out[3]);

// Whether the texture repeat (block) bx, by of tile texture `texture` is mirrored in u and in v.
struct Flip
{
    bool u = false;
    bool v = false;
};
[[nodiscard]] Flip BlockFlip(int bx, int by, int texture, int seed);

// Mirrors one tile's four texture coordinates inside its texture repeat: the repeat is the unit
// square holding the tile (FaceTexture keeps a tile inside one). Coordinates on the far edge of a
// repeat (exactly bx + 1) stay in it.
void MirrorTileUv(float uv[4][2], Flip flip);

} // namespace Render::Terrain::Variation
