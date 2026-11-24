#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

// Reforge: a theme's recolour of an interface sprite (docs/rmlui-ui-system/theming-and-modding.md,
// "Reforge"). A theme names a preset per sprite in its theme.ini [Reforge] section; the sprite's
// decoded pixels, the original's or its HD copy's, are recoloured once when RmlUi loads the texture.
// No new image files: the forged look is the game's own art with other colours.
namespace Render::Textures::Reforge
{

enum class Preset
{
    None,        // the sprite as it is
    Metal,       // frames: dark iron with antique gold on the light curls, deeper relief, a warm glow
    MetalFlat,   // thin frame pieces and buttons: the metal colours without relief or glow
    MetalBright, // buttons with a light icon (the HUD's menu buttons): metal-flat, lifted so the icon stays readable
    Cell,        // the inventory cell lattice (newui_item_box): light bronze lines
    Stone,       // window bodies (newui_msgbox_back): warm dark stone, lit from the top
    Socket,      // equipment slots: a dark recess, light bronze lines
    Gild,        // the HUD's current-skill box (partcharge1/newui_menu03): metal, its green frame gold and its
                 // green fill deep bronze
    Frame,       // window frames: metal on the light bronze ramp, so they stand off the dark stone
    FrameFlat,   // window frame strips, table edges and buttons: metal-flat on the light bronze ramp
};

// "none", "metal", "metal-flat", "metal-bright", "cell", "stone", "socket", "gild", "frame" or "frame-flat";
// nothing for any other name.
[[nodiscard]] std::optional<Preset> ParsePreset(std::string_view name);

// Recolours `pixels` in place: width x height pixels, rows top-down, 3 (RGB) or 4 (RGBA, straight
// alpha) components. `scale` is the texture's pixels per pixel of the original art: 1 for the
// original, 2 or 4 for an HD copy. The blurred parts of a preset (relief, glow, softening) are worked
// out on the original art's grid and spread back over the copy, so both copies look alike and an HD
// copy costs little more than the original. Alpha never changes, and fully transparent pixels keep
// their colour. The same input gives the same output.
void Apply(Preset preset, std::uint8_t* pixels, int width, int height, int components, int scale);

} // namespace Render::Textures::Reforge
