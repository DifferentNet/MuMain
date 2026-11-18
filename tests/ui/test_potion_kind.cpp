#include <doctest.h>

#include "UI/Mobile/PotionKind.h"

using UI::Mobile::ClassifyPotion;
using UI::Mobile::PotionKind;

TEST_CASE("an empty potion button has no kind")
{
    CHECK(ClassifyPotion(false, false, false, false, false) == PotionKind::None);
    CHECK(ClassifyPotion(false, true, false, false, false) == PotionKind::None);
}

TEST_CASE("a potion button takes the kind of the potion bound to it")
{
    CHECK(ClassifyPotion(true, true, false, false, false) == PotionKind::Heal);
    CHECK(ClassifyPotion(true, false, true, false, false) == PotionKind::Mana);
    CHECK(ClassifyPotion(true, false, false, true, false) == PotionKind::Shield);
    CHECK(ClassifyPotion(true, false, false, false, true) == PotionKind::Complex);
    CHECK(ClassifyPotion(true, false, false, false, false) == PotionKind::Other);
}

TEST_CASE("a complex potion wins over the single kinds, shield over healing and mana")
{
    CHECK(ClassifyPotion(true, true, false, true, true) == PotionKind::Complex);
    CHECK(ClassifyPotion(true, true, true, true, false) == PotionKind::Shield);
    CHECK(ClassifyPotion(true, true, true, false, false) == PotionKind::Heal);
}

TEST_CASE("the potion kinds keep the numbers the markup tests")
{
    CHECK(static_cast<int>(PotionKind::None) == 0);
    CHECK(static_cast<int>(PotionKind::Heal) == 1);
    CHECK(static_cast<int>(PotionKind::Mana) == 2);
    CHECK(static_cast<int>(PotionKind::Shield) == 3);
    CHECK(static_cast<int>(PotionKind::Complex) == 4);
}
