// Tests of "Strong monsters first" (GameLogic/Automation/TargetPriority.h): the server's /mst lines and the pick of
// a boss or a much stronger monster among the helper's targets (ghinzuro, 2026-11-08: a boss in Kalima, the hero
// kept running to the weak monsters around it).

#include <doctest.h>

#include "GameLogic/Automation/TargetPriority.h"

using GameLogic::Automation::MonsterStrengths;
using GameLogic::Automation::ParseStrengthLine;
using GameLogic::Automation::PickStrongTarget;
using GameLogic::Automation::StrengthScore;
using GameLogic::Automation::TargetCandidate;

TEST_CASE("A new list and its entries are read [gamelogic][helper-targets]")
{
    const auto line = ParseStrengthLine(L"#MST0|1200:30000000:4125|1201:9501:0");
    REQUIRE(line.has_value());
    CHECK(line->startsList);
    REQUIRE(line->entries.size() == 2);
    CHECK(line->entries[0].first == 1200);
    CHECK(line->entries[0].second.health == 30000000);
    CHECK(line->entries[0].second.damage == 4125);
    CHECK(line->entries[1].first == 1201);
    CHECK(line->entries[1].second.damage == 0);
}

TEST_CASE("An empty answer and a continued list [gamelogic][helper-targets]")
{
    const auto empty = ParseStrengthLine(L"#MST0|");
    REQUIRE(empty.has_value());
    CHECK(empty->startsList);
    CHECK(empty->entries.empty());

    const auto more = ParseStrengthLine(L"#MST|7:100:10");
    REQUIRE(more.has_value());
    CHECK_FALSE(more->startsList);
    CHECK(more->entries.size() == 1);
}

TEST_CASE("Other lines and broken entries [gamelogic][helper-targets]")
{
    CHECK_FALSE(ParseStrengthLine(L"#MSTX|1:2:3").has_value());
    CHECK_FALSE(ParseStrengthLine(L"#ADV|cp=1").has_value());
    CHECK_FALSE(ParseStrengthLine(L"Hello").has_value());

    const auto line = ParseStrengthLine(L"#MST0|1:2|x:1:1|3:-1:5|4:5:6||");
    REQUIRE(line.has_value());
    REQUIRE(line->entries.size() == 1);
    CHECK(line->entries[0].first == 4);
}

TEST_CASE("The key is the object id as the client keeps it [gamelogic][helper-targets]")
{
    const auto line = ParseStrengthLine(L"#MST0|33000:10:10");
    REQUIRE(line.has_value());
    CHECK(line->entries[0].first == (33000 & 0x7FFF));
}

TEST_CASE("A monster that never hits is no boss [gamelogic][helper-targets]")
{
    CHECK(StrengthScore({ 1000000, 0 }) == 0.0);
    CHECK(StrengthScore({ 0, 500 }) == 0.0);
    CHECK(StrengthScore({ 100, 10 }) == 1000.0);
}

TEST_CASE("The boss among weak monsters is picked [gamelogic][helper-targets]")
{
    const std::vector<TargetCandidate> around{
        { 1, 1000.0, 1 },
        { 2, 1200.0, 2 },
        { 3, 500000.0, 6 }, // the boss, farther away
    };
    CHECK(PickStrongTarget(around, -1) == 3);
    CHECK(PickStrongTarget(around, 1) == 3);
}

TEST_CASE("Monsters of about the same strength: nothing to pick [gamelogic][helper-targets]")
{
    const std::vector<TargetCandidate> around{
        { 1, 1000.0, 1 },
        { 2, 3000.0, 2 },
        { 3, 2000.0, 3 },
    };
    CHECK(PickStrongTarget(around, -1) == -1);
}

TEST_CASE("Unknown strength and a lone monster [gamelogic][helper-targets]")
{
    CHECK(PickStrongTarget({ { 1, 500000.0, 1 } }, -1) == -1);
    CHECK(PickStrongTarget({ { 1, 500000.0, 1 }, { 2, 0.0, 1 } }, -1) == -1);
    CHECK(PickStrongTarget({}, -1) == -1);
}

TEST_CASE("A strong target is kept until a much stronger one comes [gamelogic][helper-targets]")
{
    const std::vector<TargetCandidate> twoStrong{
        { 1, 1000.0, 1 },
        { 2, 10000.0, 5 },
        { 3, 20000.0, 2 },
    };
    CHECK(PickStrongTarget(twoStrong, -1) == 3);
    CHECK(PickStrongTarget(twoStrong, 2) == 2); // 3 is only twice as strong

    const std::vector<TargetCandidate> bossComes{
        { 1, 1000.0, 1 },
        { 2, 10000.0, 5 },
        { 4, 400000.0, 7 },
    };
    CHECK(PickStrongTarget(bossComes, 2) == 4);
}

TEST_CASE("Of equal strong monsters the nearer one [gamelogic][helper-targets]")
{
    const std::vector<TargetCandidate> around{
        { 1, 100.0, 1 },
        { 2, 9000.0, 6 },
        { 3, 9000.0, 3 },
    };
    CHECK(PickStrongTarget(around, -1) == 3);
}

TEST_CASE("The list follows the server's lines [gamelogic][helper-targets]")
{
    MonsterStrengths strengths;
    const auto now = std::chrono::steady_clock::now();
    CHECK(strengths.Take(L"#MST0|1:100:10|2:200:10", now));
    CHECK(strengths.Take(L"#MST|3:300:10", now));
    CHECK(strengths.ScoreOf(1) == 1000.0);
    CHECK(strengths.ScoreOf(3) == 3000.0);
    CHECK(strengths.LastAnswer() == now);

    CHECK(strengths.Take(L"#MST0|3:300:20", now));
    CHECK(strengths.ScoreOf(1) == 0.0); // a new list forgets the old one
    CHECK(strengths.ScoreOf(3) == 6000.0);

    CHECK_FALSE(strengths.Take(L"#ADV|cp=1", now));
    strengths.Clear();
    CHECK(strengths.ScoreOf(3) == 0.0);
}

TEST_CASE("The strong target stays strong once the weak ones around it are dead [gamelogic][helper-targets]")
{
    // Alone: nothing to compare with, yet the boss it fights is kept.
    CHECK(PickStrongTarget({ { 3, 500000.0, 2 } }, 3, 3) == 3);
    // Only when it is still the current target and the last strong pick.
    CHECK(PickStrongTarget({ { 3, 500000.0, 2 } }, 3, -1) == -1);
    CHECK(PickStrongTarget({ { 3, 500000.0, 2 } }, -1, 3) == -1);
    // Gone from the candidates (dead or out of range): nothing is kept.
    CHECK(PickStrongTarget({ { 1, 100.0, 1 } }, 3, 3) == -1);
    // Monsters of about its strength came: it is kept; a monster 4 times stronger takes over.
    CHECK(PickStrongTarget({ { 3, 5000.0, 2 }, { 4, 6000.0, 1 } }, 3, 3) == 3);
    CHECK(PickStrongTarget({ { 3, 5000.0, 2 }, { 4, 6000.0, 1 }, { 5, 90000.0, 4 } }, 3, 3) == 5);
}
