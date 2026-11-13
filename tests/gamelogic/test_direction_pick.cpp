#include <doctest.h>

#include "GameLogic/Touch/DirectionPick.h"

namespace Pick = GameLogic::Touch::DirectionPick;
using Pick::Candidate;

TEST_CASE("a lean picks the foe that lies that way, not the nearest")
{
    // One near on the left, one further on the right: right picks the right one.
    const std::vector<Candidate> foes{{1, -2.f, 0.f}, {2, 5.f, 0.f}};
    CHECK(Pick::Pick(foes, 0.f, 0.f, 1.f, 0.f, Pick::NoKey, true) == 2);
    CHECK(Pick::Pick(foes, 0.f, 0.f, -1.f, 0.f, Pick::NoKey, true) == 1);
}

TEST_CASE("within the cone the nearer wins, an off-angle one counts further")
{
    const std::vector<Candidate> foes{{1, 6.f, 0.f}, {2, 4.f, 3.5f}, {3, 3.f, 0.f}};
    CHECK(Pick::Pick(foes, 0.f, 0.f, 1.f, 0.f, Pick::NoKey, true) == 3);
    // Straight ahead at 6 beats 4.5 tiles off by ~41 degrees (counts ~7).
    const std::vector<Candidate> two{{1, 6.f, 0.f}, {2, 3.4f, 3.f}};
    CHECK(Pick::Pick(two, 0.f, 0.f, 1.f, 0.f, Pick::NoKey, true) == 1);
}

TEST_CASE("none in the cone: the smallest angle on a fresh lean, nothing on a held one")
{
    const std::vector<Candidate> foes{{1, 0.f, 5.f}, {2, -3.f, -3.f}};
    CHECK(Pick::Pick(foes, 0.f, 0.f, 1.f, 0.f, Pick::NoKey, true) == 1);
    CHECK(Pick::Pick(foes, 0.f, 0.f, 1.f, 0.f, Pick::NoKey, false) == Pick::NoKey);
}

TEST_CASE("a held lean goes on from the one lit and stops at the end of the row")
{
    const std::vector<Candidate> row{{1, 1.f, 0.f}, {2, 3.f, 0.f}, {3, 5.f, 0.f}};
    CHECK(Pick::Pick(row, 1.f, 0.f, 1.f, 0.f, 1, false) == 2);
    CHECK(Pick::Pick(row, 3.f, 0.f, 1.f, 0.f, 2, false) == 3);
    CHECK(Pick::Pick(row, 5.f, 0.f, 1.f, 0.f, 3, false) == Pick::NoKey);
    // The way back.
    CHECK(Pick::Pick(row, 5.f, 0.f, -1.f, 0.f, 3, false) == 2);
}

TEST_CASE("the one lit is never picked again; an empty list picks none")
{
    const std::vector<Candidate> one{{7, 2.f, 0.f}};
    CHECK(Pick::Pick(one, 2.f, 0.f, 1.f, 0.f, 7, true) == Pick::NoKey);
    CHECK(Pick::Pick({}, 0.f, 0.f, 1.f, 0.f, Pick::NoKey, true) == Pick::NoKey);
}

TEST_CASE("the walk's way tilts a distance")
{
    CHECK(Pick::Biased(4.f, 4.f, 0.f, 1.f, 0.f) == doctest::Approx(4.f));
    CHECK(Pick::Biased(4.f, 0.f, 4.f, 1.f, 0.f) == doctest::Approx(6.f));
    CHECK(Pick::Biased(4.f, -4.f, 0.f, 1.f, 0.f) == doctest::Approx(8.f));
    CHECK(Pick::Biased(4.f, -4.f, 0.f, 0.f, 0.f) == doctest::Approx(4.f));
}
