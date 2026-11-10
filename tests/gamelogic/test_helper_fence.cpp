// MU Helper by a fence (Tyres in the Arena pen at 14,52, 2026-11-10): the helper ran to and fro along the fence and
// killed nothing, with monsters on its own side, until it was switched off and on.
//
// The walk into range of a target (GameLogic::Automation::AttackObject / CastSkill) asks the path finder for a way
// to within the hunting range plus the reach of the target. The target is within the hunting range already, so every
// free tile round the character ends the path: each call is one step to the neighbouring tile nearest to the target.
// With a wall between, that is a step along the wall and the next one back. The result is "approaching" every time,
// never "no path" or "blocked", so the target was never given up.

#include "stdafx.h"

#include "doctest.h"

#include "Engine/Pathing/ZzzPath.h"
#include "GameLogic/Automation/Attack.h"

#include <array>
#include <set>
#include <utility>

using GameLogic::Automation::ApproachWatch;
using GameLogic::Automation::IsBetterTarget;

namespace
{
constexpr int kMapSize = 24;
constexpr int kWallY = 10;       // a wall across the whole map, as the pen's fence at y 51
constexpr float kEndRange = 8.f; // hunting range 6 + a reach of 2, as the helper asks
constexpr long long kRound = 250;
constexpr long long kPatience = 2000;
constexpr long long kPause = 1000;

struct Tile
{
    int x;
    int y;
};

// One step of the walk into range, as ApproachAlong takes it: the path's first step from the character's tile.
// Returns false when the path finder has none.
bool StepTowards(std::array<WORD, kMapSize * kMapSize>& map, Tile& hero, const Tile& target)
{
    map[hero.y * kMapSize + hero.x] |= TW_CHARACTER;
    PATH path;
    path.SetMapDimensions(kMapSize, kMapSize, map.data());
    const bool found = path.FindPath(hero.x, hero.y, target.x, target.y, true, TW_CHARACTER, false, kEndRange);
    map[hero.y * kMapSize + hero.x] &= static_cast<WORD>(~TW_CHARACTER);
    if (!found || path.GetPath() < 2)
    {
        return false;
    }
    hero = {path.GetPathX()[1], path.GetPathY()[1]};
    return true;
}

std::array<WORD, kMapSize * kMapSize> MapWithWall()
{
    std::array<WORD, kMapSize * kMapSize> map{};
    for (int x = 0; x < kMapSize; ++x)
    {
        map[kWallY * kMapSize + x] = TW_NOMOVE;
    }
    return map;
}

int Gap(const Tile& a, const Tile& b)
{
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}
} // namespace

TEST_CASE("The walk to a target behind a wall steps to and fro along it and never gives a reason to stop")
{
    auto map = MapWithWall();
    Tile hero{12, kWallY + 1};
    const Tile target{12, kWallY - 3}; // 4 tiles away, out of a reach of 2

    std::set<std::pair<int, int>> visited;
    for (int round = 0; round < 40; ++round)
    {
        REQUIRE(StepTowards(map, hero, target)); // "approaching", every round
        visited.insert({hero.x, hero.y});
        CHECK(hero.y > kWallY); // never across
    }
    CHECK(visited.size() <= 3); // the same one or two tiles by the wall
}

TEST_CASE("The watch gives such a walk up after its patience")
{
    auto map = MapWithWall();
    Tile hero{12, kWallY + 3};
    const Tile target{12, kWallY - 3};

    ApproachWatch watch;
    long long now = 100000;
    long long gaveUpAfter = -1;
    for (int round = 0; round < 40 && gaveUpAfter < 0; ++round, now += kRound)
    {
        REQUIRE(StepTowards(map, hero, target));
        if (watch.NoteStep(7, Gap(hero, target), now, kPatience, kPause))
        {
            gaveUpAfter = now - 100000;
        }
    }
    // Two steps up to the wall count as getting closer; then nothing does.
    CHECK(gaveUpAfter >= kPatience);
    CHECK(gaveUpAfter <= kPatience + 4 * kRound);
}

TEST_CASE("A walk across open ground is never given up")
{
    std::array<WORD, kMapSize * kMapSize> map{};
    Tile hero{2, 20};
    const Tile target{20, 3};

    ApproachWatch watch;
    long long now = 5000;
    // A slow walker: a step every second round.
    for (int round = 0; round < 60 && Gap(hero, target) > 4; ++round, now += kRound)
    {
        if (round % 2 == 0)
        {
            REQUIRE(StepTowards(map, hero, target));
        }
        CHECK_FALSE(watch.NoteStep(7, Gap(hero, target), now, kPatience, kPause));
    }
    CHECK(Gap(hero, target) <= 4);
}

TEST_CASE("A new target or a walk taken up again after a pause starts the watch over")
{
    ApproachWatch watch;
    CHECK_FALSE(watch.NoteStep(1, 16, 0, kPatience, kPause));
    CHECK_FALSE(watch.NoteStep(1, 16, 1000, kPatience, kPause));
    CHECK_FALSE(watch.NoteStep(2, 25, 1900, kPatience, kPause)); // another target
    CHECK_FALSE(watch.NoteStep(2, 25, 2900, kPatience, kPause));
    CHECK(watch.NoteStep(2, 25, 3900, kPatience, kPause));
    // Given up: the next step is a new approach.
    CHECK_FALSE(watch.NoteStep(2, 25, 4150, kPatience, kPause));

    ApproachWatch paused;
    CHECK_FALSE(paused.NoteStep(1, 16, 0, kPatience, kPause));
    CHECK_FALSE(paused.NoteStep(1, 16, 900, kPatience, kPause));
    CHECK_FALSE(paused.NoteStep(1, 16, 900 + kPause + 1, kPatience, kPause)); // fought something else meanwhile
    CHECK_FALSE(paused.NoteStep(1, 16, 900 + kPause + 1 + kPatience - 1, kPatience, kPause - 1 + kPatience));
}

TEST_CASE("A target in the open comes before a nearer one behind a wall")
{
    CHECK(IsBetterTarget(true, 5, false, 2));
    CHECK_FALSE(IsBetterTarget(false, 2, true, 5));
    // Among equals the nearer one, and the later one at the same distance.
    CHECK(IsBetterTarget(true, 2, true, 3));
    CHECK_FALSE(IsBetterTarget(true, 4, true, 3));
    CHECK(IsBetterTarget(false, 3, false, 3));
}
