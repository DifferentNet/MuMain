#include <doctest.h>

#include "UI/Character/StatPointsMenu.h"

#include <vector>

using namespace UI::Character::StatPoints;

TEST_CASE("no free points, no steps [ui][stats]")
{
    CHECK(MenuAmounts(0).empty());
    CHECK(MenuAmounts(-3).empty());
}

TEST_CASE("the steps below all the points, then all of them [ui][stats]")
{
    CHECK(MenuAmounts(1) == std::vector<int>{1});
    CHECK(MenuAmounts(10) == std::vector<int>{10});
    CHECK(MenuAmounts(11) == std::vector<int>{10, 11});
    CHECK(MenuAmounts(500) == std::vector<int>{10, 100, 500});
    CHECK(MenuAmounts(1000) == std::vector<int>{10, 100, 1000});
    CHECK(MenuAmounts(65535) == std::vector<int>{10, 100, 1000, 65535});
}

TEST_CASE("one command per stat in the order of the + buttons [ui][stats]")
{
    CHECK(Command(0, 10) == L"/addstr 10");
    CHECK(Command(1, 100) == L"/addagi 100");
    CHECK(Command(2, 1000) == L"/addvit 1000");
    CHECK(Command(3, 7) == L"/addene 7");
    CHECK(Command(4, 500) == L"/addcmd 500");
}

TEST_CASE("no command for an unknown stat or no points [ui][stats]")
{
    CHECK(Command(-1, 10).empty());
    CHECK(Command(5, 10).empty());
    CHECK(Command(0, 0).empty());
}
