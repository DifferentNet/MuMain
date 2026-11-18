// Where the pad's and the touch controls' area skills aim (pad round 6, B6, ghinzuro 2026-11-18).
//
// A mage's area skill cast by a button went to the lit foe, wherever the pack around it stood: Flame
// on the one stray at the edge hit one. Now the skills that strike an area around a tile (Flame,
// Cometfall, Decay, Ice Storm) go on the foe within the skill's reach that has the most others around
// it, and the skills that fly along a line (Twister, Aqua Beam) go the way the stick points. The
// mouse's own aim, the server's ranges and the skills' reach stay as they are: only which foe or tile
// the button picks changes. The sums only; GameLogic/Touch/TouchActions reads the foes and casts.
#pragma once

#include <optional>
#include <span>

namespace GameLogic::Touch::AreaAim
{
constexpr int NoKey = -1;

struct Foe
{
    int key = NoKey;
    // Where it stands, in the map's whole tiles (as the server counts an area's hits).
    int tileX = 0;
    int tileY = 0;
    // A player, not a monster: a pack of one kind never pulls a cast off a foe of the other.
    bool player = false;
    // Within the skill's reach of the hero, with no wall between: a cast on it may go off.
    bool castable = false;
};

// How the server counts the foes an area skill strikes around the tile it lands on.
enum class AreaRule
{
    // Nearer than `size` tiles in a straight line (TargetAreaDiameter / 2: Flame, Cometfall, Ice Storm).
    Round,
    // At most `size` tiles off on each axis, a square (EffectRange with no target area filter: Decay).
    Square,
};

// The foe to cast on so the area takes the most foes: among the castable ones, the one with the most
// foes in its area (AreaRule, `size`), itself and any not castable counted. Only foes of the lit foe's
// kind count and may be chosen (monsters with none lit). A tie goes to the one nearest the lit foe
// `litKey` (the lit one itself first), with none lit to the one nearest the hero. NoKey with no
// castable foe of that kind.
[[nodiscard]] int PackCentre(std::span<const Foe> foes, int heroX, int heroY, AreaRule rule, float size, int litKey);

// A step from the hero's tile, in tiles.
struct Step
{
    int dx = 0;
    int dy = 0;
};

// The tile `tiles` ahead the way (dirX, dirY) points from the hero, rounded to the grid; none when the
// way is no way at all, `tiles` is below one, or the rounded tile lies past `range`.
[[nodiscard]] std::optional<Step> AlongLine(float dirX, float dirY, int tiles, float range);
} // namespace GameLogic::Touch::AreaAim
