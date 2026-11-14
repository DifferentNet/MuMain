#include "doctest.h"

#include "GameLogic/Skills/SkillDamagePercent.h"

namespace SkillDamagePercent = GameLogic::Skills::SkillDamagePercent;

TEST_CASE("The Dark Knight's skill damage grows with energy up to 7,000")
{
    CHECK(SkillDamagePercent::Knight(0) == 200);
    CHECK(SkillDamagePercent::Knight(9) == 200);
    CHECK(SkillDamagePercent::Knight(1000) == 300);
    CHECK(SkillDamagePercent::Knight(6999) == 899);
    CHECK(SkillDamagePercent::Knight(7000) == 900);
    CHECK(SkillDamagePercent::Knight(7001) == 900);
    CHECK(SkillDamagePercent::Knight(65535) == 900);
    CHECK(SkillDamagePercent::KnightMaxPercent == 900);
}

TEST_CASE("The Magic Gladiator's skill damage is 150 %")
{
    CHECK(SkillDamagePercent::MagicGladiator() == 150);
}

TEST_CASE("The Dark Lord's skill damage grows with energy up to 12,000")
{
    CHECK(SkillDamagePercent::DarkLord(0) == 100);
    CHECK(SkillDamagePercent::DarkLord(4000) == 200);
    CHECK(SkillDamagePercent::DarkLord(11999) == 399);
    CHECK(SkillDamagePercent::DarkLord(12000) == 400);
    CHECK(SkillDamagePercent::DarkLord(12001) == 400);
    CHECK(SkillDamagePercent::DarkLord(65535) == 400);
    CHECK(SkillDamagePercent::DarkLordMaxPercent == 400);
}

TEST_CASE("A negative energy counts as none")
{
    CHECK(SkillDamagePercent::Knight(-1) == 200);
    CHECK(SkillDamagePercent::DarkLord(-1) == 100);
}
