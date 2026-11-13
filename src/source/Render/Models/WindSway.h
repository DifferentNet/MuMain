// WindSway.h: grass, bushes and tree crowns sway in the wind in the Remaster (desktop, HD textures
// on), from the player's [Render] Wind settings.
//
//   Map objects: a mesh of a world object whose texture is one of the foliage pictures below
//   (kFoliage, by file name, so it holds on every map) bends in skinned_textured.vert: the higher a
//   vertex stands above the object's origin, the further it moves along the wind direction, a slow
//   wave plus a faster gust, its phase travelling across the map. The same uniforms reach the sun
//   shadow and the SSAO depth, so their shapes sway too. Terrain grass keeps its CPU quads; with the
//   wind on, each tile's top edge moves by GrassOffset instead of the Classic column wave.
//   With the wind off (strength 0) the uniforms stay 0, the shader returns at once and the grass
//   draws the Classic lines.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

class BMD;
class OBJECT;

namespace Render::Models::Wind
{

inline constexpr float kPercentPerUnit = 100.0f;

// The wind blows along this horizontal direction (unit length); skinned_textured.vert uses the same.
inline constexpr float kDirectionX = 0.6f;
inline constexpr float kDirectionY = 0.8f;

// What a mesh of a map object is, from its texture.
enum class Kind : std::uint8_t
{
    None,
    // Tree crowns and branches: tall, a slow wide sway.
    Crown,
    // Bushes, plants and tall grass models.
    Bush,
    // Flowers: short, a small sway.
    Flower,
};

// The kind of a BMD texture name ("tree_04.tga", or a path ending in one; either separator, any
// case). Only .tga pictures sway: the .jpg ones are solid parts (trunks, stones, walls).
[[nodiscard]] Kind ClassifyTexture(std::string_view textureName);

// How a mesh sways: the shader's uniforms.
struct Sway
{
    // The largest offset, in world units (a terrain tile is 100); 0 = no sway.
    float amplitude = 0.0f;
    // 1 / the height above the object's origin over which the sway grows to full.
    float inverseHeightRange = 0.0f;
};

// The sway of a kind on an object of this scale at this strength (0..1). A bigger object grows over
// a taller range and sways a little more (its scale counts 0.5..1.5).
[[nodiscard]] Sway SwayFor(Kind kind, float objectScale, float strength);

// The wind's phase for the frame, 0..4 pi (both of the shader's waves line up again there).
[[nodiscard]] float Phase(double worldTimeMs);

// How far (world units) the top edge of terrain grass at tile (x, y) moves along the wind
// direction (0.6, 0.8), at this phase and strength (0..1); at most 12 * strength either way.
[[nodiscard]] float GrassOffset(float phase, int x, int y, float strength);

// The strength (0..1) for the saved settings: 0 when not available (no HD textures, the browser
// build), on the Low graphics preset, for Wind 0 or an unknown value, and at strength 0.
// strengthPercent is clamped to 0..100.
[[nodiscard]] inline float Resolve(int wind, int strengthPercent, bool available, bool lowPreset)
{
    if (!available || lowPreset || wind != 1)
    {
        return 0.0f;
    }
    return static_cast<float>(std::clamp(strengthPercent, 0, 100)) / kPercentPerUnit;
}

// Why it does not show right now (the Options window's Wind row).
enum class Block : std::uint8_t
{
    None,
    // HD textures are off in this session (they switch on with the next start).
    NoHdTextures,
    // The Low graphics preset keeps it off.
    LowPreset,
};

// The first reason that applies, in the order of Block.
[[nodiscard]] inline Block ResolveBlock(bool hdTextures, bool lowPreset)
{
    if (!hdTextures)
    {
        return Block::NoHdTextures;
    }
    if (lowPreset)
    {
        return Block::LowPreset;
    }
    return Block::None;
}

// Whether a reason locks the Options row.
[[nodiscard]] inline bool Locks(Block block)
{
    return block != Block::None;
}

// Engine side (WindSetting.cpp).

// Whether this build can draw it (desktop only).
[[nodiscard]] bool Available();
// The [Render] Wind settings, at start-up once the graphics preset and HD textures are known, and
// again by the Options window (a change of graphics preset). Logs only when the strength changes.
void ApplyFromConfig();
// The strength in use (0..1); 0 = the wind is off.
[[nodiscard]] float Strength();
// Why it does not show right now, from the session's HD textures and graphics preset.
[[nodiscard]] Block CurrentBlock();
// Saves [Render] Wind and WindStrength (Options > Graphics) and applies them at once.
void Choose(bool enabled, int strengthPercent);

// While alive, the meshes of this map object's own model may sway (MeshSway). Only around the
// object's own draw in RenderWorldObject: its effects, the UI and previews never sway.
class ScopedObject
{
public:
    explicit ScopedObject(const OBJECT* object);
    ~ScopedObject();
    ScopedObject(const ScopedObject&) = delete;
    ScopedObject& operator=(const ScopedObject&) = delete;

private:
    bool m_active = false;
};

// The shader's wind uniforms for this mesh of `model`: amplitude, 1 / height range and phase.
// False (out untouched) when no world object is being drawn, the model is not that object's own,
// the mesh is no foliage or the wind is off.
[[nodiscard]] bool MeshSway(const BMD* model, int meshIndex, float out[3]);

} // namespace Render::Models::Wind
