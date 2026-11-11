#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// The Remaster's normal maps: each HD texture of a model (items, characters, monsters, NPCs and
// map objects) gets a map of its relief, built from its own pixels when it loads. Darker texels
// count as lower (crevices, seams and engraving are painted dark), so the per-pixel light of the
// Enhanced lighting (skinned_lit.frag) picks out the relief on flat polygons. Nothing is stored on
// disk; Classic (HD off) never builds one.
namespace Render::Textures::NormalMaps
{

// Read once from the config at start-up, like HD textures.
void SetEnabled(bool enabled);
[[nodiscard]] bool Enabled();

// For development only (MU_NORMAL_MAPS_RUNTIME=1, off by default): build a texture's map at load
// when its prebuilt file is missing, as before the maps shipped.
void SetRuntimeBuild(bool enabled);
[[nodiscard]] bool RuntimeBuild();

// Whether the texture at `path` (an HD copy, "Data\HD\...") belongs to a model that gets a normal
// map: Item, Player, Monster, NPC and ObjectN. World ground, effects, skills and the interface
// do not, nor the characters' faces (Player skin_*, SkinClassNNN, LevelClassNNN, *head_N, *face*):
// their shading is painted in. Either separator and any case. Mirrored by scripts/hd_pack.py.
[[nodiscard]] bool WantsNormalMap(std::wstring_view path);

// The normal map of `rgba` (width x height, at full size; scripts/hd_pack.py does the same and then halves it) RGBA8, rows top to bottom): red and green hold the
// tilt along u (right) and v (down), 128 = flat; blue and alpha are 255. `scale` is how many
// times the HD copy was upscaled (1 or more): the broad relief is measured over that many texels.
// Fully transparent texels count as the lowest. Empty for an empty image.
[[nodiscard]] std::vector<std::uint8_t> Build(std::span<const std::uint8_t> rgba, int width, int height,
                                              int scale);

} // namespace Render::Textures::NormalMaps
