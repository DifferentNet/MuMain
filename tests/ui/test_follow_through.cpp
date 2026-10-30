#include <doctest.h>

#include "UI/Mobile/FollowThrough.h"

using UI::Mobile::FollowThrough;

namespace
{
constexpr int Attack = 3;
constexpr int Skill = 7;
} // namespace

TEST_CASE("nothing is owed before a press")
{
    const FollowThrough follow;
    CHECK(follow.Owed(0.0) == FollowThrough::None);
}

TEST_CASE("a tap owes its action until it went off")
{
    FollowThrough follow;
    follow.Press(Attack, 10.0);
    CHECK(follow.Owed(10.0) == Attack);
    CHECK(follow.Owed(11.5) == Attack);
    follow.Settle(Attack);
    CHECK(follow.Owed(11.6) == FollowThrough::None);
}

TEST_CASE("an action that keeps waiting is given up after a few seconds")
{
    FollowThrough follow;
    follow.Press(Skill, 10.0);
    CHECK(follow.Owed(10.0 + FollowThrough::MaxSeconds) == Skill);
    CHECK(follow.Owed(10.0 + FollowThrough::MaxSeconds + 0.1) == FollowThrough::None);
}

TEST_CASE("a newer tap takes the place of an older one")
{
    FollowThrough follow;
    follow.Press(Attack, 10.0);
    follow.Press(Skill, 10.2);
    CHECK(follow.Owed(10.3) == Skill);
    // The attack settling later does not cancel the skill.
    follow.Settle(Attack);
    CHECK(follow.Owed(10.4) == Skill);
}

TEST_CASE("a second tap on the same button starts its time again")
{
    FollowThrough follow;
    follow.Press(Skill, 10.0);
    follow.Press(Skill, 13.0);
    CHECK(follow.Owed(16.5) == Skill);
}

TEST_CASE("a drop forgets the owed action")
{
    FollowThrough follow;
    follow.Press(Attack, 10.0);
    follow.Drop();
    CHECK(follow.Owed(10.0) == FollowThrough::None);
}
