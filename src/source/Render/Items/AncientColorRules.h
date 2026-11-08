#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// The colours of the ancient sets (MU Skynet, ghinzuro 2026-11-07, palette "A"): every ancient set sits on the
// model of a normal set (Kantata and Rave both on Plate), so its armour is drawn in a colour of its own. The
// textures of the model are recoloured when a piece of the set is first drawn (Render/Items/AncientColors.h), and
// the ancient pulse takes the same colour. These rules need nothing of the game, so the tests run them.
namespace Render::Items::AncientColors
{
// How a texture takes the set's colour.
enum class Tint : std::uint8_t
{
    // The whole texture to grey, brightened, times the colour: for plain metal models (Plate, Bone, Adamantine).
    Colorize,
    // Only the coloured parts take the set's hue; silver, white and black trims stay: for two-tone models
    // (Dragon, Valiant, Glorious, Red Wing).
    Hue,
    // Only the model's main colour (its hue range, AccentRange) takes the set's hue; gold trims, gems and silver
    // keep the original look: for models with a main colour and accents (Thunder Hawk, Grand Soul, Legendary,
    // Dark Steel, Eclipse; ghinzuro 2026-11-07: one hue all over looked like a mess).
    Accent,
};

struct SetColor
{
    int set;  // the set's number in itemsettype.bmd (CSItemOption)
    float r;
    float g;
    float b;
    Tint tint;
};

// The colour of a set by its number; nullptr for a set without one (not an ancient set, unknown).
const SetColor* FindSetColor(int set);

// The set of an armour piece from the two set numbers of its item (itemsettype.bmd) and the server's ancient
// discriminator (1 or 2). The client file has a few pieces in the other slot (Spirit helm and boots of Karis): when
// the slot of the discriminator is empty, the other number counts. 0 when there is none.
int PickSet(const std::array<std::uint8_t, 2>& setsOfItem, int discriminator);

// A plain piece (no ancient set) drawn in the colour of the set the character's armour shows: pieces of a model
// whose ancient set has no such piece, so they don't stand out next to it (Grand Soul pants with Alvis armour;
// ghinzuro 2026-11-08, option A: looks only, no stats). `itemType` is group * 512 + number. 0 when it keeps its look.
int BorrowedSet(int itemType, int armourSet);

// Whether a texture of an armour model keeps its colours: skin, hair, faces and the hidden meshes (by file name).
bool KeepsTexture(std::wstring_view fileName);

// Recolours pixels in place: `count` pixels of `components` bytes (3 RGB or 4 RGBA; alpha stays). Pixels in skin
// tones keep their colour (bare arms and faces painted into some armour textures).
void Recolor(std::uint8_t* pixels, std::size_t count, int components, const SetColor& color);
} // namespace Render::Items::AncientColors
