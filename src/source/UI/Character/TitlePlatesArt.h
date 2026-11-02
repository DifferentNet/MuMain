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

// The effects atlas, Data/Interface/newui_titles_fx.OZT: white layers, their shape in the alpha, to be added
// and tinted. Each plate's silhouette (the shine) and glow (around the edge; it covers the plate and glowPad
// of the plates' atlas pixels more on every side), in effects atlas pixels; then the sparkle and the ember.
inline constexpr float FxAtlasWidth = 1024.f;
inline constexpr float FxAtlasHeight = 512.f;

struct Rect
{
    float x, y, width, height;
};

struct PlateFx
{
    Rect silhouette;
    Rect glow;
    float glowPad;
};

inline constexpr PlateFx PlateEffects[6] = {
    // silver
    {{4.f, 4.f, 312.f, 58.f}, {512.f, 4.f, 176.f, 49.f}, 40.06f},
    // ice
    {{4.f, 66.f, 312.f, 64.f}, {512.f, 57.f, 176.f, 52.f}, 40.06f},
    // jade
    {{4.f, 134.f, 401.f, 73.f}, {512.f, 113.f, 220.f, 56.f}, 40.1f},
    // gold
    {{4.f, 211.f, 432.f, 74.f}, {512.f, 173.f, 236.f, 57.f}, 40.05f},
    // crimson
    {{4.f, 289.f, 490.f, 88.f}, {512.f, 234.f, 265.f, 64.f}, 40.04f},
    // violet
    {{4.f, 381.f, 347.f, 75.f}, {512.f, 302.f, 193.f, 57.f}, 40.17f},
};

inline constexpr Rect Sparkle = {800.f, 8.f, 64.f, 64.f};
inline constexpr Rect Ember = {880.f, 8.f, 32.f, 32.f};

// The gems drawn on each plate, where the sparkles go: their centres in the plates' atlas pixels from the plate's
// top left corner, the main one first.
inline constexpr int MaxGems = 4;

struct Point
{
    float x, y;
};

struct Gems
{
    int count;
    Point at[MaxGems];
};

inline constexpr Gems PlateGems[6] = {
    // silver
    {0, {}},
    // ice
    {1, {{312.5f, 12.9f}}},
    // jade
    {1, {{401.f, 30.66f}}},
    // gold
    {3, {{432.5f, 25.16f}, {373.68f, 32.56f}, {491.32f, 32.56f}}},
    // crimson
    {4, {{490.5f, 35.2f}, {447.34f, 13.2f}, {490.5f, 5.28f}, {533.66f, 13.2f}}},
    // violet
    {3, {{347.5f, 24.f}, {291.9f, 30.f}, {403.1f, 30.f}}},
};
} // namespace UI::TitlePlates::Art
