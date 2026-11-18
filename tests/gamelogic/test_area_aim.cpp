#include <doctest.h>

#include "GameLogic/Touch/AreaAim.h"

#include <vector>

namespace AreaAim = GameLogic::Touch::AreaAim;
using AreaAim::AreaRule;
using AreaAim::Foe;

namespace
{
// Flame, Cometfall and Ice Storm hit nearer than half their area's diameter of 3 tiles.
constexpr float FlameRadius = 1.5f;
// Decay strikes within EffectRange 2 on each axis (OpenMU GetAttackablesInRange, no target area filter).
constexpr float DecayRange = 2.f;

Foe Monster(int key, int x, int y, bool castable = true)
{
    return {key, x, y, false, castable};
}

Foe Player(int key, int x, int y, bool castable = true)
{
    return {key, x, y, true, castable};
}

int Flame(const std::vector<Foe>& foes, int lit)
{
    return AreaAim::PackCentre(foes, 0, 0, AreaRule::Round, FlameRadius, lit);
}
} // namespace

TEST_CASE("the cast goes on the foe with the most others around it [gamelogic][area-aim]")
{
    // A stray near the hero, a pack of three further off.
    const std::vector<Foe> foes{Monster(1, 2, 0), Monster(2, 5, 0), Monster(3, 6, 0), Monster(4, 5, 1)};
    CHECK(Flame(foes, AreaAim::NoKey) == 2);
    // The stray lit: the pack still wins, it holds more.
    CHECK(Flame(foes, 1) == 2);
}

TEST_CASE("a tie goes to the lit foe, then the one nearest it [gamelogic][area-aim]")
{
    // Two pairs alike: the lit foe's pair wins, the lit one itself first.
    const std::vector<Foe> foes{Monster(1, 2, 0), Monster(2, 3, 0), Monster(3, 0, 4), Monster(4, 0, 5)};
    CHECK(Flame(foes, 4) == 4);
    CHECK(Flame(foes, 2) == 2);
    // None lit: the one nearest the hero.
    CHECK(Flame(foes, AreaAim::NoKey) == 1);
}

TEST_CASE("a foe out of reach or behind a wall is never the centre, yet it counts around one [gamelogic][area-aim]")
{
    // The pack's middle stands out of reach: the cast goes on the castable one next to it.
    const std::vector<Foe> foes{Monster(1, 6, 0), Monster(2, 7, 0, false), Monster(3, 8, 0, false), Monster(4, 1, 3)};
    CHECK(Flame(foes, 4) == 1);
    // Nothing castable: no centre.
    const std::vector<Foe> walled{Monster(1, 2, 0, false), Monster(2, 3, 0, false)};
    CHECK(Flame(walled, 1) == AreaAim::NoKey);
    CHECK(Flame({}, AreaAim::NoKey) == AreaAim::NoKey);
}

TEST_CASE("the round area counts nearer than the radius, the square one each axis [gamelogic][area-aim]")
{
    // A diagonal neighbour (1.41 tiles) is inside Flame's 1.5; two tiles off is not.
    const std::vector<Foe> foes{Monster(1, 2, 0), Monster(2, 4, 0), Monster(3, 0, 3), Monster(4, 1, 4)};
    CHECK(Flame(foes, 1) == 3);
    // Decay's square takes the pair two apart as well: then the lit one's pair.
    CHECK(AreaAim::PackCentre(foes, 0, 0, AreaRule::Square, DecayRange, 1) == 1);
    // Two tiles off on both axes is in Decay's square, not in a circle of 2.
    const std::vector<Foe> corner{Monster(1, 1, 0), Monster(2, 5, 5), Monster(3, 7, 7), Monster(4, 3, 7)};
    CHECK(AreaAim::PackCentre(corner, 0, 0, AreaRule::Square, DecayRange, 1) == 2);
}

TEST_CASE("only foes of the lit one's kind count and may be chosen [gamelogic][area-aim]")
{
    // A crowd of players beside a lone monster: the monster stays the aim.
    const std::vector<Foe> foes{Monster(1, 2, 0), Player(2, 5, 0), Player(3, 6, 0), Player(4, 5, 1)};
    CHECK(Flame(foes, 1) == 1);
    // With none lit, monsters only.
    CHECK(Flame(foes, AreaAim::NoKey) == 1);
    // A player lit: a pack of monsters does not pull the cast off the players.
    const std::vector<Foe> mixed{Player(1, 2, 0), Monster(2, 5, 0), Monster(3, 6, 0), Player(4, 0, 4)};
    CHECK(Flame(mixed, 1) == 1);
    // Players about and no monster: nothing for a monster pack.
    const std::vector<Foe> players{Player(1, 2, 0), Player(2, 3, 0)};
    CHECK(Flame(players, AreaAim::NoKey) == AreaAim::NoKey);
    // Monsters standing among the players do not add to a player's count.
    const std::vector<Foe> among{Player(1, 2, 0), Monster(2, 2, 1), Monster(3, 3, 0), Player(4, 0, 5), Player(5, 0, 6)};
    CHECK(Flame(among, 1) == 4);
}

TEST_CASE("the line goes the way the stick points, within the reach [gamelogic][area-aim]")
{
    const auto right = AreaAim::AlongLine(1.f, 0.f, 4, 6.f);
    REQUIRE(right.has_value());
    CHECK(right->dx == 4);
    CHECK(right->dy == 0);
    // Not normalised in: the length of the way does not matter.
    const auto diagonal = AreaAim::AlongLine(-3.f, 3.f, 4, 6.f);
    REQUIRE(diagonal.has_value());
    CHECK(diagonal->dx == -3);
    CHECK(diagonal->dy == 3);
    // Rounded past the reach: none.
    CHECK_FALSE(AreaAim::AlongLine(1.f, 1.f, 6, 5.5f).has_value());
    CHECK(AreaAim::AlongLine(1.f, 1.f, 4, 5.5f).has_value());
}

TEST_CASE("no line for a still stick or no step [gamelogic][area-aim]")
{
    CHECK_FALSE(AreaAim::AlongLine(0.f, 0.f, 4, 6.f).has_value());
    CHECK_FALSE(AreaAim::AlongLine(1.f, 0.f, 0, 6.f).has_value());
}
