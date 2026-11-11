// MU Helper in Tier Maps (ghinzuro, 2026-11-11: «mu helper тоже странно себя ведет в тир мапс начинает куда то
// убегать»): the fortress's gates and statue are monsters to the client, but the server lets a gate be hit only
// while its zone allows it (ImperialGuardianGate.IsAttackable), and sends nothing back for a hit on a closed one.
// The helper walked to the next zone's gates or kept hitting a closed one. Gates and the statue are now targets of
// last resort, and one whose hits bring no damage packet for a while is left alone.

#include "stdafx.h"

#include "doctest.h"

#include "GameLogic/Automation/Attack.h"

using GameLogic::Automation::BlindHitWatch;
using GameLogic::Automation::FortressAwareRank;
using GameLogic::Automation::IsBetterFortressRank;
using GameLogic::Automation::IsFortressObjectType;

namespace
{
constexpr long long kPatience = 3000;
constexpr long long kPause = 1500;
constexpr long long kSwing = 300; // a Death Stab every 300 ms
constexpr int kGate = 1201;
constexpr int kOtherGate = 1202;
}

TEST_CASE("Only the fortress's gates and statue are fortress objects")
{
    CHECK(IsFortressObjectType(524));
    CHECK(IsFortressObjectType(525));
    CHECK(IsFortressObjectType(526));
    CHECK(IsFortressObjectType(527));
    CHECK(IsFortressObjectType(528));
    CHECK_FALSE(IsFortressObjectType(523)); // the trap: invisible, never sent
    CHECK_FALSE(IsFortressObjectType(529));
    CHECK_FALSE(IsFortressObjectType(504)); // a fortress boss stays an ordinary target
    CHECK_FALSE(IsFortressObjectType(275)); // Illusion of Kundun 7
}

TEST_CASE("The fortress rank: clear line first, then not a fortress object")
{
    CHECK(FortressAwareRank(false, true) == 0);  // a monster in the open
    CHECK(FortressAwareRank(true, true) == 1);   // a gate in the open
    CHECK(FortressAwareRank(false, false) == 2); // a monster behind a wall
    CHECK(FortressAwareRank(true, false) == 3);  // a gate behind a wall
}

TEST_CASE("A guard behind the standing gate never wins over the gate")
{
    // Arguments: candidate fortress, candidate clear line, best fortress, best clear line, otherwise better.
    // Candidate is the guard behind the gate, nearer, the best so far the gate in the open: the gate stays.
    CHECK_FALSE(IsBetterFortressRank(false, false, true, true, true));
    // Candidate is the gate in the open, farther, the best so far the guard behind it: the gate wins.
    CHECK(IsBetterFortressRank(true, true, false, false, false));
}

TEST_CASE("A monster in the open comes before a gate in the open, whatever the distance")
{
    CHECK(IsBetterFortressRank(false, true, true, true, false));
    CHECK_FALSE(IsBetterFortressRank(true, true, false, true, true));
}

TEST_CASE("Behind a wall a monster still comes before a gate")
{
    CHECK(IsBetterFortressRank(false, false, true, false, false));
    CHECK_FALSE(IsBetterFortressRank(true, false, false, false, true));
}

TEST_CASE("At the same rank the distance rule decides")
{
    for (const bool fortress : { false, true })
    {
        for (const bool clearLine : { false, true })
        {
            CHECK(IsBetterFortressRank(fortress, clearLine, fortress, clearLine, true));
            CHECK_FALSE(IsBetterFortressRank(fortress, clearLine, fortress, clearLine, false));
        }
    }
}

TEST_CASE("A closed gate is given up after the patience of hits without damage")
{
    BlindHitWatch watch;
    long long now = 0;
    bool givenUp = false;
    for (; now < 10000 && !givenUp; now += kSwing)
    {
        givenUp = watch.NoteHit(kGate, now, kPatience, kPause);
    }
    CHECK(givenUp);
    CHECK(now - kSwing >= kPatience);
    CHECK(now - kSwing < kPatience + kSwing);
}

TEST_CASE("An open gate that answers every hit is never given up")
{
    BlindHitWatch watch;
    for (long long now = 0; now < 60000; now += kSwing)
    {
        CHECK_FALSE(watch.NoteHit(kGate, now, kPatience, kPause));
        watch.NoteDamage(kGate, now + 120); // the server's answer, a ping later
    }
}

TEST_CASE("Misses count as answers, damage for another target doesn't")
{
    BlindHitWatch watch;
    long long now = 0;
    bool givenUp = false;
    for (; now < 10000 && !givenUp; now += kSwing)
    {
        givenUp = watch.NoteHit(kGate, now, kPatience, kPause);
        watch.NoteDamage(kOtherGate, now + 100); // a splash on another gate is no answer for this one
    }
    CHECK(givenUp);
}

TEST_CASE("A pause or another target starts the watch over")
{
    BlindHitWatch watch;
    CHECK_FALSE(watch.NoteHit(kGate, 0, kPatience, kPause));
    CHECK_FALSE(watch.NoteHit(kGate, 2700, kPatience, kPause)); // 2.7 s: a pause over 1.5 s, starts over
    CHECK_FALSE(watch.NoteHit(kGate, 3000, kPatience, kPause));
    CHECK_FALSE(watch.NoteHit(kOtherGate, 3300, kPatience, kPause)); // another gate: starts over
    CHECK_FALSE(watch.NoteHit(kOtherGate, 5000, kPatience, kPause)); // pause again
    for (long long now = 5300; now < 8000; now += kSwing)
    {
        CHECK_FALSE(watch.NoteHit(kOtherGate, now, kPatience, kPause));
    }
    CHECK(watch.NoteHit(kOtherGate, 8000, kPatience, kPause));
}
