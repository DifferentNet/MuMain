// Tests of the buff countdowns: a relog or character switch must not keep the last character's timers (round 15, B8).

#include "stdafx.h"

#include "doctest.h"

#include "GameLogic/Buffs/w_BuffTimeControl.h"

namespace
{
constexpr eBuffTimeType kFirstTimer = static_cast<eBuffTimeType>(1006);
constexpr eBuffTimeType kSecondTimer = static_cast<eBuffTimeType>(1007);
constexpr DWORD kMsPerSecond = 1000;
} // namespace

TEST_CASE("Clear drops every buff countdown [gamelogic][buff-time]")
{
    BuffTimeControlPtr control = BuffTimeControl::Make();
    control->StartBuffTimer(kFirstTimer, eBuff_Hellowin1, 60);
    control->StartBuffTimer(kSecondTimer, eBuff_Hellowin2, 120);
    REQUIRE(control->IsBuffTime(kFirstTimer));
    REQUIRE(control->IsBuffTime(kSecondTimer));

    control->Clear();

    CHECK_FALSE(control->IsBuffTime(kFirstTimer));
    CHECK_FALSE(control->IsBuffTime(kSecondTimer));
    CHECK(control->GetBuffTime(kFirstTimer) == 0);
}

TEST_CASE("a buff started again takes the new time [gamelogic][buff-time]")
{
    BuffTimeControlPtr control = BuffTimeControl::Make();
    control->StartBuffTimer(kFirstTimer, eBuff_Hellowin1, 30);
    control->StartBuffTimer(kFirstTimer, eBuff_Hellowin1, 600);

    CHECK(control->GetBuffTime(kFirstTimer) == 600 * kMsPerSecond);
    control->Clear();
}

TEST_CASE("after Clear a buff starts with its own time [gamelogic][buff-time]")
{
    BuffTimeControlPtr control = BuffTimeControl::Make();
    control->StartBuffTimer(kFirstTimer, eBuff_Hellowin1, 30);
    control->Clear();
    control->StartBuffTimer(kFirstTimer, eBuff_Hellowin1, 90);

    CHECK(control->GetBuffTime(kFirstTimer) == 90 * kMsPerSecond);
    control->Clear();
}
