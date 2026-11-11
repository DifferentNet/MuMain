// MU Helper in Tier Maps (ghinzuro, 2026-11-11: «mu helper тоже странно себя ведет в тир мапс начинает куда то
// убегать»): the fortress's gates and statue are monsters to the client, but the server lets a gate be hit only
// while its zone allows it (ImperialGuardianGate.IsAttackable), and sends nothing back for a hit on a closed one.
// The helper walked to the next zone's gates or kept hitting a closed one. Gates and the statue are now targets of
// last resort, and one whose hits bring no damage packet for a while is left alone.

#include "stdafx.h"

#include "doctest.h"

#include "GameLogic/Automation/Attack.h"

using GameLogic::Automation::BlindHitWatch;
using GameLogic::Automation::IsBetterFortressAware;
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

TEST_CASE("Any monster comes before a fortress object, whatever the distance")
{
    // Candidate is a guard far off, the best so far a gate next to the hero: the guard wins.
    CHECK(IsBetterFortressAware(false, true, false));
    // Candidate is a gate, the best so far a guard: the gate never wins.
    CHECK_FALSE(IsBetterFortressAware(true, false, true));
    // Two of a kind: the usual rule decides.
    CHECK(IsBetterFortressAware(false, false, true));
    CHECK_FALSE(IsBetterFortressAware(false, false, false));
    CHECK(IsBetterFortressAware(true, true, true));
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
