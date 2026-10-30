#include <doctest.h>

#include "GameLogic/Touch/SteerHold.h"

using GameLogic::Touch::HeroPlace;
using GameLogic::Touch::IsJump;
using GameLogic::Touch::SteerHold;

namespace
{
constexpr int Lorencia = 0;
constexpr int Dungeon = 1;
constexpr double Frame = 1.0 / 60.0;
} // namespace

TEST_CASE("a walk is no jump, a gate is")
{
    const HeroPlace start{Lorencia, 120, 220, 10.0};
    CHECK_FALSE(IsJump(start, {Lorencia, 121, 220, 10.0 + Frame}));
    CHECK_FALSE(IsJump(start, {Lorencia, 128, 228, 10.0 + Frame}));
    CHECK(IsJump(start, {Lorencia, 140, 220, 10.0 + Frame}));
    CHECK(IsJump(start, {Lorencia, 120, 200, 10.0 + Frame}));
}

TEST_CASE("a slow frame lets the hero cover more ground before it counts as a jump")
{
    const HeroPlace start{Lorencia, 120, 220, 10.0};
    // A second's stall on a weak phone: the hero walked on meanwhile.
    CHECK_FALSE(IsJump(start, {Lorencia, 135, 220, 11.0}));
    CHECK(IsJump(start, {Lorencia, 150, 220, 11.0}));
}

TEST_CASE("a clock that went back counts as no time")
{
    CHECK(IsJump({Lorencia, 0, 0, 10.0}, {Lorencia, 9, 0, 9.0}));
    CHECK_FALSE(IsJump({Lorencia, 0, 0, 10.0}, {Lorencia, 8, 0, 9.0}));
}

TEST_CASE("a held stick walks on along the way")
{
    SteerHold hold;
    double t = 0.0;
    for (int tile = 100; tile < 120; ++tile, t += 0.3)
        CHECK(hold.MayWalk({Lorencia, tile, 50, t}));
    CHECK_FALSE(hold.IsWaiting());
}

TEST_CASE("a held stick waits after a map change until it is let go")
{
    SteerHold hold;
    CHECK(hold.MayWalk({Lorencia, 120, 220, 1.0}));
    CHECK_FALSE(hold.MayWalk({Dungeon, 108, 247, 1.5}));
    CHECK(hold.IsWaiting());
    // Still held: it keeps waiting, wherever the hero is.
    CHECK_FALSE(hold.MayWalk({Dungeon, 108, 247, 2.0}));
    CHECK_FALSE(hold.MayWalk({Dungeon, 109, 247, 2.1}));

    hold.Release();
    CHECK_FALSE(hold.IsWaiting());
    CHECK(hold.MayWalk({Dungeon, 108, 247, 2.5}));
}

TEST_CASE("a held stick waits after a gate inside the map")
{
    SteerHold hold;
    CHECK(hold.MayWalk({Dungeon, 230, 30, 1.0}));
    CHECK_FALSE(hold.MayWalk({Dungeon, 30, 120, 1.0 + Frame}));
    CHECK(hold.IsWaiting());
}

TEST_CASE("a fresh push after a let-go is no jump, wherever the hero went meanwhile")
{
    SteerHold hold;
    CHECK(hold.MayWalk({Lorencia, 120, 220, 1.0}));
    hold.Release();
    // A tap on the map walked the hero far away, and the stick is pushed again.
    CHECK(hold.MayWalk({Dungeon, 10, 10, 60.0}));
}

TEST_CASE("controls that went away under a held stick make it wait")
{
    SteerHold hold;
    CHECK(hold.MayWalk({Lorencia, 120, 220, 1.0}));
    hold.Pause();
    CHECK(hold.IsWaiting());
    CHECK_FALSE(hold.MayWalk({Lorencia, 120, 220, 3.0}));
    hold.Release();
    CHECK(hold.MayWalk({Lorencia, 120, 220, 3.5}));
}

TEST_CASE("a pause with no stick held leaves the next push free")
{
    SteerHold hold;
    hold.Pause();
    CHECK_FALSE(hold.IsWaiting());
    CHECK(hold.MayWalk({Lorencia, 120, 220, 1.0}));
}

TEST_CASE("a push stays held while it waits, and only a let-go ends it")
{
    SteerHold hold;
    CHECK_FALSE(hold.IsHeld());
    CHECK(hold.MayWalk({Lorencia, 120, 220, 1.0}));
    CHECK(hold.IsHeld());
    CHECK_FALSE(hold.MayWalk({Dungeon, 108, 247, 1.5}));
    CHECK(hold.IsHeld());
    hold.Release();
    CHECK_FALSE(hold.IsHeld());
}
