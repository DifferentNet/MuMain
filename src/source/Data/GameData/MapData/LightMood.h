#pragma once

#include "Data/GameData/ItemData/ItemDataIssue.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The light mood of the world maps (Data/Maps/LightMood.json):
//
//   { "formatVersion": 1,
//     "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 0.6 },
//     "maps": [ { "world": 10, "name": "Icarus", "background": 0.7 } ] }
//
// Each value scales the Classic light of a world map:
//   base         the static (baked) light of the terrain and the map objects, 0.2 to 1;
//   figures      the static light on characters, monsters and NPCs, from base to 1;
//   dynamicGain  the light of candles, fires, skills and effects, 0.5 to 2;
//   background   the clear and fog colour behind the scene, 0 to 1;
//   sun          the directional light on models (buildings, rocks, figures):
//                how far a face turned to it gets brighter than one turned
//                away, 0 (none, only the even fill light) to 1 (Classic).
//   shadowKeep   optional, 0 (default) to 1.5: the baked light at which a place
//                keeps its full brightness. base and figures then darken the
//                shade only, fading out towards this light (sunlit ground stays
//                day); 0 darkens everything evenly.
//   shadeCool    optional, 0 (default) to 0.5: how much bluer the darkened
//                shade gets (red and green darken a little more).
//   brightScale  optional, 0.3 to 1 (default, Classic): the scale places at the
//                shadow keep get, never below base (figures: never below the
//                figure scale); only with a shadow keep.
//   sunAzimuth   optional, 0 to 360 degrees (default 225): where the sun of the
//                sun shadows stands, counted from the map's +X axis towards +Y.
//                225 throws the shadows to the side of the default camera; at 315
//                (the Classic terrain light) they fell straight behind each figure,
//                hidden by its own body.
//   sunElevation optional, 20 to 85 degrees (default 40): its height above the
//                horizon; low suns cast long shadows.
//   sunShadow    optional, 0 to 1 (default 0.8): the share of the light a
//                place in full sun shadow loses; 0 casts none on the map.
//   sunSoftness  optional, 2 to 40 (default 4): the soft edge of the
//                shadows, in world units (a tile is 100), the same at every
//                shadow map size.
//   haze         optional, 0 (default, none) to 0.5: the Remaster's distance
//                haze (Render/PostProcessing/HazeSetting.h), how much of the
//                haze colour the far world takes before the fog.
//   hazeR, hazeG, hazeB  optional, 0 to 1 (default 0.60, 0.65, 0.72): its
//                colour, darkened by background.
//   groundFog    optional, 0 (default, none) to 0.5: the low mist over the
//                ground below the hero's height plus groundFogHeight.
//   groundFogHeight  optional, 50 to 600 (default 220): how high above the
//                hero's ground that mist reaches, in world units.
//   hazeSun      optional, 0 (default) to 0.5: how much brighter the haze
//                gets looking towards the sun (sunAzimuth).
//   particles    optional, 0 (default, none) to 6: the kind of the Remaster's
//                first layer of ambient air particles
//                (Render/Effects/ParticlesSetting.h), by number: 1 dust,
//                2 pollen, 3 fireflies, 4 fine snow, 5 ash, 6 embers (rounded;
//                anything else is none).
//   particlesDensity  optional, 0 to 2 (default 1): how many of them, times
//                the kind's own count.
//   particles2, particles2Density  optional, the same for a second layer.
//   particlesR, particlesG, particlesB  optional, 0 to 1 (default 1, 1, 1):
//                the tint of both layers.
// "default" has all five and applies to every map without an entry. A map
// entry names its world number and overrides any of them; "name" is for the
// reader only.
namespace Data::Maps
{
struct LightMood
{
    double base = 1.0;
    double figures = 1.0;
    double dynamicGain = 1.0;
    double background = 1.0;
    double sun = 1.0;
    double shadowKeep = 0.0;
    double shadeCool = 0.0;
    double brightScale = 1.0;
    double sunAzimuth = 225.0;
    double sunElevation = 40.0;
    double sunShadow = 0.8;
    double sunSoftness = 4.0;
    double haze = 0.0;
    double hazeR = 0.60;
    double hazeG = 0.65;
    double hazeB = 0.72;
    double groundFog = 0.0;
    double groundFogHeight = 220.0;
    double hazeSun = 0.0;
    double particles = 0.0;
    double particlesDensity = 1.0;
    double particles2 = 0.0;
    double particles2Density = 1.0;
    double particlesR = 1.0;
    double particlesG = 1.0;
    double particlesB = 1.0;

    bool operator==(const LightMood&) const = default;
};

struct MapLightMood
{
    int world = 0;
    LightMood mood;

    bool operator==(const MapLightMood&) const = default;
};

struct LightMoodData
{
    LightMood defaultMood;
    std::vector<MapLightMood> maps;

    // The world's own mood, or the default.
    const LightMood& ForWorld(int world) const;
};

constexpr int LightMoodFormatVersion = 1;

// The darkest base: the figure light divides by it.
constexpr double MinLightMoodBase = 0.2;
constexpr double MinLightMoodDynamicGain = 0.5;
constexpr double MaxLightMoodDynamicGain = 2.0;
// The server sends the map number as one byte.
constexpr int MaxLightMoodWorld = 255;

// One value of a mood: its key in the file, the member it fills, whether the
// default must name it, and its range. "figures" has the effective base as its
// lower bound on top of `min`.
struct LightMoodValueInfo
{
    const char* key;
    double LightMood::* member;
    bool required;
    double min;
    double max;
};

constexpr std::size_t LightMoodValueCount = 26;
[[nodiscard]] const std::array<LightMoodValueInfo, LightMoodValueCount>& LightMoodValueInfos();
// The value with that key in the file, if there is one.
[[nodiscard]] const LightMoodValueInfo* FindLightMoodValue(std::string_view key);

// The file, relative to the client folder.
constexpr const char* LightMoodFile = "Data/Maps/LightMood.json";

// Reads the moods of the file into `data`. One invalid value makes the whole
// file unusable: `data` is then left Classic (every value 1) and `issues`
// has errors (Items::HasErrors).
void ReadLightMoodJson(std::string_view text, const std::string& source, LightMoodData& data,
                       std::vector<Items::ItemDataIssue>& issues);

struct LightMoodLoadResult
{
    LightMoodData data;
    std::vector<Items::ItemDataIssue> issues;
};

// Reads the file; one that cannot be read is an error.
LightMoodLoadResult LoadLightMoodFile(const std::filesystem::path& file);
} // namespace Data::Maps
