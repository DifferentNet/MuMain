#pragma once

class CHARACTER;
class OBJECT;

// The battle pass' season aura (stage 6, the reward of the pass' level 45): a slowly turning rune circle under the
// character's feet in the season's colour and the season's particles (Santa's snow in the winter season) falling
// around it. Who wears which season comes from GameLogic/Events/SeasonMarks.h, how a season looks from
// Data/Seasons.json.
//
// Drawn from RenderCharacter's effects of the players, next to the full set's glow and on the same terms: not in
// Chaos Castle (everyone looks alike there), not on a character under invisibility, and only from the graphics level
// that draws the full set's glow. The circle is a different texture (Effect/mzine_typer2) and colour than the game
// master's mark (Effect/gmmzine, white with a violet light), so the two are not taken for each other.
namespace Render::Effects
{
void RenderSeasonAura(CHARACTER* c, OBJECT* o);
}
