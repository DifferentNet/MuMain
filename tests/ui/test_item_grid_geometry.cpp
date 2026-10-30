#include "doctest.h"
#include "UI/Inventory/ItemGridGeometry.h"

#include <cmath>

using UI::Items::AnchoredTargetPoint;
using UI::Items::AnchoredTopLeft;
using UI::Items::GridGeometry;
using UI::Items::PickupAnchor;

TEST_CASE("an item grid finds the cell under a point at the original pitch")
{
    const GridGeometry grid{100.f, 200.f, 20.f, 20.f, 8, 4};
    int column = -1;
    int row = -1;

    REQUIRE(grid.CellAt(100.f, 200.f, column, row));
    CHECK(column == 0);
    CHECK(row == 0);
    REQUIRE(grid.CellAt(119.f, 219.f, column, row));
    CHECK(column == 0);
    CHECK(row == 0);
    REQUIRE(grid.CellAt(120.f, 220.f, column, row));
    CHECK(column == 1);
    CHECK(row == 1);
    REQUIRE(grid.CellAt(259.f, 279.f, column, row));
    CHECK(column == 7);
    CHECK(row == 3);

    CHECK_FALSE(grid.CellAt(260.f, 210.f, column, row));
    CHECK_FALSE(grid.CellAt(99.f, 210.f, column, row));
    CHECK_FALSE(grid.CellAt(110.f, 280.f, column, row));
}

TEST_CASE("an item grid counts cells off its edges as the original did")
{
    const GridGeometry grid{100.f, 200.f, 20.f, 20.f, 8, 4};
    // For every offset the original's (d / 20) - (d < 0 ? 1 : 0), in integer arithmetic.
    for (int d = -45; d <= 200; ++d)
    {
        int column = 0;
        int row = 0;
        grid.CellOf(100.f + d, 200.f + d, column, row);
        const int expected = d / 20 - (d < 0 ? 1 : 0);
        CHECK(column == (d >= 0 && d < 160 ? d / 20 : expected));
        CHECK(row == (d >= 0 && d < 80 ? d / 20 : expected));
    }
}

TEST_CASE("an item grid follows the theme's pitch")
{
    const GridGeometry grid{10.f, 10.f, 24.f, 26.f, 8, 8};
    int column = -1;
    int row = -1;
    REQUIRE(grid.CellAt(10.f + 24.f * 3 + 23.f, 10.f + 26.f * 5, column, row));
    CHECK(column == 3);
    CHECK(row == 5);

    const auto box = grid.CellsRect(2, 1, 2, 3);
    CHECK(box.x == doctest::Approx(58.f));
    CHECK(box.y == doctest::Approx(36.f));
    CHECK(box.width == doctest::Approx(48.f));
    CHECK(box.height == doctest::Approx(78.f));

    const auto bounds = grid.Bounds();
    CHECK(bounds.width == doctest::Approx(192.f));
    CHECK(bounds.height == doctest::Approx(208.f));
}

TEST_CASE("an item grid without a laid-out pitch keeps the original's")
{
    const GridGeometry grid{0.f, 0.f, 0.f, -1.f, 8, 8};
    CHECK(grid.PitchX() == GridGeometry::DefaultPitch);
    CHECK(grid.PitchY() == GridGeometry::DefaultPitch);
}

TEST_CASE("a dragged item keeps its hold across grid pitches")
{
    // Held 37 x 13 px into a 2 x 2 item on a 20 pitch.
    const PickupAnchor anchor{37.f / 20.f, 13.f / 20.f};
    int left = 0;
    int top = 0;

    AnchoredTopLeft(500.f, 300.f, anchor, 20.f, 20.f, left, top);
    CHECK(left == 463);
    CHECK(top == 287);

    // The same point of the item on a 40 pitch: 74 x 26 in.
    AnchoredTopLeft(500.f, 300.f, anchor, 40.f, 40.f, left, top);
    CHECK(left == 426);
    CHECK(top == 274);

    // Every whole-pixel hold on a 20 pitch round-trips exactly.
    for (int offset = 0; offset < 80; ++offset)
    {
        const PickupAnchor hold{offset / 20.f, offset / 20.f};
        AnchoredTopLeft(1000.f, 1000.f, hold, 20.f, 20.f, left, top);
        CHECK(left == 1000 - offset);
        CHECK(top == 1000 - offset);
    }
}

TEST_CASE("a dragged item goes to the cell its top-left square covers the most")
{
    // A grid laid out off whole pixels, 32 px cells (20 at 1.6x), like the trade window's.
    const GridGeometry grid{418.7f, 260.3f, 32.f, 32.f, 8, 4};
    int x = 0;
    int y = 0;
    int column = -1;
    int row = -1;

    // Held by its middle and put down with the pointer on the middle of each cell: that cell, the
    // first row and column included. The item's top-left corner lies on the cell border then, a
    // pixel outside the grid on its first column.
    const float holds[] = {15.f / 32.f, 0.5f, 17.f / 32.f};
    for (const float hold : holds)
    {
        const PickupAnchor anchor{hold, hold};
        for (int cell = 0; cell < 4; ++cell)
        {
            const auto square = grid.CellsRect(cell, cell, 1, 1);
            AnchoredTargetPoint(std::floor(square.x + square.width / 2.f), std::floor(square.y + square.height / 2.f),
                                anchor, grid.PitchX(), grid.PitchY(), x, y);
            REQUIRE(grid.CellAt(static_cast<float>(x), static_cast<float>(y), column, row));
            CHECK(column == cell);
            CHECK(row == cell);
        }
    }

    // Held by its top-left corner, an item a third of a cell right of cell 2 still covers cell 2 the most.
    const PickupAnchor corner{0.f, 0.f};
    AnchoredTargetPoint(418.7f + 2 * 32.f + 10.f, 260.3f + 5.f, corner, 32.f, 32.f, x, y);
    REQUIRE(grid.CellAt(static_cast<float>(x), static_cast<float>(y), column, row));
    CHECK(column == 2);
    CHECK(row == 0);

    // A 2 x 2 item held by its middle (the original's equipment pickup): its top-left square is the
    // cell up and left of the pointer, as the original's MouseX - (Width - 1) * 20 / 2.
    const PickupAnchor middle{1.f, 1.f};
    AnchoredTargetPoint(418.7f + 3 * 32.f + 1.f, 260.3f + 2 * 32.f + 1.f, middle, 32.f, 32.f, x, y);
    REQUIRE(grid.CellAt(static_cast<float>(x), static_cast<float>(y), column, row));
    CHECK(column == 2);
    CHECK(row == 1);
}
