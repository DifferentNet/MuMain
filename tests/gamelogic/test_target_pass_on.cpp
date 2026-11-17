#include <doctest.h>

#include "GameLogic/Touch/TargetPassOn.h"

namespace PassOn = GameLogic::Touch::TargetPassOn;
using GameLogic::Touch::DirectionPick::Candidate;
using GameLogic::Touch::DirectionPick::NoKey;
using Clock = PassOn::Clock;

TEST_CASE("a young or fought choice passes on, an old one left alone does not")
{
    const Clock::time_point now = Clock::now();
    const Clock::time_point longAgo = now - std::chrono::minutes(1);
    CHECK(PassOn::Passes(now - std::chrono::seconds(4), longAgo, false, now));
    CHECK_FALSE(PassOn::Passes(now - std::chrono::seconds(11), longAgo, false, now));
    // Chosen long ago, but fought a moment ago, or the hero fighting still.
    CHECK(PassOn::Passes(longAgo, now - std::chrono::seconds(1), false, now));
    CHECK(PassOn::Passes(longAgo, longAgo, true, now));
}

TEST_CASE("it passes on the way the dead foe lay, not to the nearest behind")
{
    // The hero at 0,0; the dead one lay east. One near to the west, one further to the east.
    const std::vector<Candidate> foes{{1, -2.f, 0.f}, {2, 5.f, 1.f}};
    CHECK(PassOn::Next(foes, 0.f, 0.f, 4.f, 0.f, 9) == 2);
    // Lay west: the west one.
    CHECK(PassOn::Next(foes, 0.f, 0.f, -3.f, 0.f, 9) == 1);
}

TEST_CASE("the nearest that way first, never the one gone, and none with none that way")
{
    const std::vector<Candidate> foes{{9, 3.f, 0.f}, {1, 6.f, 0.f}, {2, 3.5f, 0.5f}};
    CHECK(PassOn::Next(foes, 0.f, 0.f, 3.f, 0.f, 9) == 2);
    // Only one behind the hero: the choice is dropped, not turned round.
    const std::vector<Candidate> behind{{1, -4.f, 0.f}};
    CHECK(PassOn::Next(behind, 0.f, 0.f, 3.f, 0.f, 9) == NoKey);

    CHECK(PassOn::Next({}, 0.f, 0.f, 3.f, 0.f, 9) == NoKey);
    // Dead on the hero's own tile: the plain nearest.
    const std::vector<Candidate> around{{1, -5.f, 0.f}, {2, 0.f, 2.f}};
    CHECK(PassOn::Next(around, 0.f, 0.f, 0.f, 0.f, 9) == 2);
}
