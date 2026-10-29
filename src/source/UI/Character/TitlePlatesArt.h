#pragma once

// Written by tools/make_title_plates.ps1 together with Data/Interface/newui_titles.OZT; change the tool, not this file.
// Where each rarity's plate lies in the atlas and its parts (UI/Character/TitlePlates.cpp), in atlas pixels:
// the plate's place, its core's top and bottom, where the left end stops and the right end starts, where the
// middle (gems, crown) starts and stops, how far the text may reach on each side, and the straight column
// the game stretches on each side; all but the place are from the plate's top left corner.
namespace UI::TitlePlates::Art
{
inline constexpr float AtlasSize = 1024.f;

struct Plate
{
    float x, y, width, height;
    float coreTop, coreBottom;
    float capLeft, capRight;
    float decoLeft, decoRight;
    float textLeft, textRight;
    float cleanLeft, cleanRight;
};

inline constexpr Plate Plates[6] = {
    // silver
    {8.f, 8.f, 641.f, 120.f, 20.48f, 100.34f, 82.02f, 558.98f, 316.19f, 324.81f, 51.26f, 589.74f, 236.5f, 386.5f},
    // ice
    {8.f, 144.f, 595.f, 123.f, 28.56f, 108.53f, 76.14f, 518.86f, 275.99f, 319.01f, 44.54f, 550.46f, 211.5f, 384.5f},
    // jade
    {8.f, 288.f, 933.f, 170.f, 68.93f, 149.04f, 204.65f, 728.35f, 420.92f, 512.08f, 169.76f, 763.24f, 369.5f, 664.5f},
    // gold
    {8.f, 480.f, 874.f, 149.f, 48.46f, 128.41f, 202.13f, 671.87f, 345.64f, 528.36f, 169.79f, 704.21f, 293.5f, 583.5f},
    // crimson
    {8.f, 648.f, 957.f, 171.f, 74.01f, 153.86f, 273.21f, 683.79f, 405.91f, 551.09f, 241.2f, 715.8f, 372.5f, 577.5f},
    // violet
    {8.f, 840.f, 720.f, 156.f, 51.86f, 131.94f, 120.14f, 599.86f, 267.2f, 452.8f, 89.07f, 630.93f, 240.5f, 486.5f},
};
} // namespace UI::TitlePlates::Art
