#include <doctest.h>

#include "UI/Chat/ZenChangeWindow.h"

using UI::Chat::ZenChangeWindow;

TEST_CASE("repair all: one sum after the last money update [ui][chat]")
{
    ZenChangeWindow repair{ZenChangeWindow::Sign::Loss, {0, 3'000, 500}};
    repair.Arm(1'000);
    CHECK(repair.Note(-1'200, 1'050));
    CHECK(repair.Note(-800, 1'060));
    CHECK(repair.Note(-500, 1'070));
    CHECK_FALSE(repair.Poll(1'200).has_value());
    const auto total = repair.Poll(1'600);
    REQUIRE(total.has_value());
    CHECK(*total == 2'500);
    CHECK_FALSE(repair.Poll(1'700).has_value());
    CHECK_FALSE(repair.Armed());
}

TEST_CASE("a window counts only its own sign and only while armed [ui][chat]")
{
    ZenChangeWindow sale{ZenChangeWindow::Sign::Gain, {0, 3'000, 400}};
    CHECK_FALSE(sale.Note(5'000, 100));
    sale.Arm(200);
    CHECK_FALSE(sale.Note(-300, 250));
    CHECK_FALSE(sale.Note(0, 260));
    CHECK(sale.Note(7'000, 300));
    CHECK(sale.Poll(800) == 7'000);
}

TEST_CASE("nothing changed: the window ends without a sum [ui][chat]")
{
    ZenChangeWindow repair{ZenChangeWindow::Sign::Loss, {0, 3'000, 500}};
    repair.Arm(0);
    CHECK_FALSE(repair.Poll(2'000).has_value());
    CHECK(repair.Armed());
    CHECK_FALSE(repair.Poll(3'001).has_value());
    CHECK_FALSE(repair.Armed());
    // A change after the window is not counted.
    CHECK_FALSE(repair.Note(-100, 3'500));
}

TEST_CASE("a death counts a loss that came just before it [ui][chat]")
{
    ZenChangeWindow death{ZenChangeWindow::Sign::Loss, {1'500, 2'500, 600}};
    CHECK_FALSE(death.Note(-40'000, 10'000));
    death.Arm(10'400);
    CHECK_FALSE(death.Poll(10'900).has_value());
    CHECK(death.Poll(11'000) == 40'000);

    // Too long before the death: not counted.
    CHECK_FALSE(death.Note(-1'000, 20'000));
    death.Arm(22'000);
    CHECK_FALSE(death.Poll(24'000).has_value());
    CHECK_FALSE(death.Poll(24'501).has_value());
}

TEST_CASE("the loss may also come just after the death [ui][chat]")
{
    ZenChangeWindow death{ZenChangeWindow::Sign::Loss, {1'500, 2'500, 600}};
    death.Arm(5'000);
    CHECK(death.Note(-12'345, 5'300));
    CHECK_FALSE(death.Poll(5'800).has_value());
    CHECK(death.Poll(5'900) == 12'345);
}

TEST_CASE("arming again keeps the sum [ui][chat]")
{
    ZenChangeWindow sale{ZenChangeWindow::Sign::Gain, {0, 3'000, 400}};
    sale.Arm(0);
    CHECK(sale.Note(1'000, 100));
    sale.Arm(200);
    CHECK(sale.Note(2'000, 300));
    CHECK(sale.Poll(700) == 3'000);
}

TEST_CASE("a refused sale stops the window [ui][chat]")
{
    ZenChangeWindow sale{ZenChangeWindow::Sign::Gain, {0, 3'000, 400}};
    sale.Arm(0);
    sale.Stop();
    CHECK_FALSE(sale.Armed());
    // A zen pickup right after is not taken for the sale.
    CHECK_FALSE(sale.Note(500, 100));
    CHECK_FALSE(sale.Poll(1'000).has_value());

    // Refused after an earlier sale of the same burst: that sum is still said, and nothing more is added.
    sale.Arm(2'000);
    CHECK(sale.Note(1'000, 2'100));
    sale.Arm(2'200);
    sale.Stop();
    CHECK_FALSE(sale.Note(700, 2'250));
    CHECK(sale.Poll(2'260) == 1'000);
    CHECK_FALSE(sale.Armed());
}

TEST_CASE("the first change must come in time [ui][chat]")
{
    ZenChangeWindow repair{ZenChangeWindow::Sign::Loss, {0, 3'000, 500, 1'000}};
    repair.Arm(0);
    CHECK_FALSE(repair.Poll(1'000).has_value());
    CHECK(repair.Armed());
    CHECK_FALSE(repair.Poll(1'001).has_value());
    CHECK_FALSE(repair.Armed());
    CHECK_FALSE(repair.Note(-100, 1'200));

    // Once the first change came, later ones count for the whole window.
    repair.Arm(5'000);
    CHECK(repair.Note(-100, 5'900));
    CHECK(repair.Note(-200, 6'300));
    CHECK(repair.Poll(6'800) == 300);
}

TEST_CASE("tick counters wrap around [ui][chat]")
{
    ZenChangeWindow repair{ZenChangeWindow::Sign::Loss, {0, 3'000, 500}};
    repair.Arm(0xFFFF'FF00u);
    CHECK(repair.Note(-10, 0x0000'0010u));
    CHECK(repair.Poll(0x0000'0300u) == 10);
}
