#pragma once

// Written by tools/make_title_plates.ps1 together with Data/Interface/newui_titles.OZT; change the tool, not this file.
// Where each rarity's plate lies in the atlas and its field (the coloured rectangle the text sits on), in atlas
// pixels: the plate's place, then its field's left, top, right and bottom from the plate's top left corner. All
// plates have the same scale, so their fields are the same size: FieldWidth x FieldHeight on average.
namespace UI::TitlePlates::Art
{
inline constexpr float AtlasSize = 1024.f;
inline constexpr float FieldWidth = 566.84f;
inline constexpr float FieldHeight = 73.85f;

struct Plate
{
    float x, y, width, height;
    float fieldLeft, fieldTop, fieldRight, fieldBottom;
};

inline constexpr Plate Plates[6] = {
    // silver
    {8.f, 8.f, 625.f, 117.f, 28.39f, 22.36f, 596.61f, 97.43f},
    // ice
    {8.f, 144.f, 625.f, 129.f, 27.19f, 33.15f, 597.81f, 109.83f},
    // jade
    {8.f, 296.f, 802.f, 146.f, 117.94f, 53.6f, 684.06f, 127.6f},
    // gold
    {8.f, 464.f, 865.f, 148.f, 149.23f, 52.54f, 716.57f, 124.74f},
    // crimson
    {8.f, 632.f, 981.f, 176.f, 209.24f, 80.98f, 772.96f, 153.95f},
    // violet
    {8.f, 824.f, 695.f, 150.f, 64.78f, 49.47f, 629.82f, 121.68f},
};
} // namespace UI::TitlePlates::Art
