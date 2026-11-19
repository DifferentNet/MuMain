#include "doctest.h"

#include "GameLogic/Events/EntryLevels.h"

using namespace GameLogic::Events::EntryLevels;

TEST_CASE("Blood Castle bands match the entry window's table")
{
    CHECK(GradeCount(Event::BloodCastle) == 8);
    CHECK(BandOf(Event::BloodCastle, false, 0).minLevel == 15);
    CHECK(BandOf(Event::BloodCastle, false, 0).maxLevel == 80);
    CHECK(BandOf(Event::BloodCastle, true, 0).minLevel == 10);
    CHECK(BandOf(Event::BloodCastle, true, 6).minLevel == 311);
    CHECK(BandOf(Event::BloodCastle, false, 8).maxLevel == 0);
    CHECK(BandOf(Event::BloodCastle, false, -1).maxLevel == 0);
}

TEST_CASE("The hero's castle follows the level and the class")
{
    CHECK(GradeFor(Event::BloodCastle, false, false, 14) == -1);
    CHECK(GradeFor(Event::BloodCastle, false, false, 15) == 0);
    CHECK(GradeFor(Event::BloodCastle, false, false, 131) == 2);
    CHECK(GradeFor(Event::BloodCastle, false, false, 400) == 6);
    CHECK(GradeFor(Event::BloodCastle, true, false, 10) == 0);
    CHECK(GradeFor(Event::BloodCastle, true, false, 61) == 1);
    // The third class goes to castle 8 from its band on, and below it takes the castle of its level.
    CHECK(GradeFor(Event::BloodCastle, false, true, 331) == 7);
    CHECK(GradeFor(Event::BloodCastle, false, true, 330) == 5);
    CHECK(GradeFor(Event::BloodCastle, true, true, 311) == 7);
}

TEST_CASE("Devil Square bands and grades")
{
    CHECK(GradeCount(Event::DevilSquare) == 7);
    CHECK(BandOf(Event::DevilSquare, false, 0).maxLevel == 130);
    CHECK(BandOf(Event::DevilSquare, true, 0).maxLevel == 110);
    CHECK(GradeFor(Event::DevilSquare, false, false, 130) == 0);
    CHECK(GradeFor(Event::DevilSquare, false, false, 131) == 1);
    CHECK(GradeFor(Event::DevilSquare, true, false, 111) == 1);
    CHECK(GradeFor(Event::DevilSquare, false, true, 331) == 6);
    CHECK(GradeFor(Event::DevilSquare, false, false, 331) == 5);
    CHECK(GradeFor(Event::DevilSquare, false, false, 1) == -1);
}
