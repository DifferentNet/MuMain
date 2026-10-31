#include <doctest.h>

#include "UI/HUD/PadTargetPick.h"

#include <vector>

namespace Pick = UI::HUD::PadTargetPick;
using Pick::Box;
using Pick::Target;

namespace
{
constexpr float Cell = 20.f;

// A grid of `columns` x `rows` cells from (left, top), row by row, in window `window`.
std::vector<Target> Grid(float left, float top, int columns, int rows, int window = 0)
{
    std::vector<Target> cells;
    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const float x = left + column * Cell;
            const float y = top + row * Cell;
            cells.push_back({{x, y, x + Cell, y + Cell}, window});
        }
    }
    return cells;
}
} // namespace

TEST_CASE("At takes the smallest target under the point")
{
    const std::vector<Target> targets{{{0.f, 0.f, 100.f, 100.f}}, {{10.f, 10.f, 30.f, 30.f}}};
    CHECK(Pick::At(targets, 20.f, 20.f) == 1);
    CHECK(Pick::At(targets, 50.f, 50.f) == 0);
    CHECK(Pick::At(targets, 150.f, 50.f) == Pick::None);
}

TEST_CASE("Nearest snaps only within reach")
{
    const std::vector<Target> targets = Grid(0.f, 0.f, 2, 1);
    CHECK(Pick::Nearest(targets, 45.f, 10.f, 10.f) == 1);
    CHECK(Pick::Nearest(targets, 70.f, 10.f, 10.f) == Pick::None);
    CHECK(Pick::Nearest(targets, 5.f, 5.f, 0.f) == 0);
}

TEST_CASE("The d-pad steps cell by cell through a grid")
{
    const std::vector<Target> grid = Grid(0.f, 0.f, 4, 3);
    // Cell (1, 1) is index 5.
    CHECK(Pick::NextThatWay(grid, 5, 1, 0) == 6);
    CHECK(Pick::NextThatWay(grid, 5, -1, 0) == 4);
    CHECK(Pick::NextThatWay(grid, 5, 0, -1) == 1);
    CHECK(Pick::NextThatWay(grid, 5, 0, 1) == 9);
    // Nothing past the edge.
    CHECK(Pick::NextThatWay(grid, 3, 1, 0) == Pick::None);
    CHECK(Pick::NextThatWay(grid, 0, 0, -1) == Pick::None);
}

TEST_CASE("The d-pad keeps to the row before it turns aside")
{
    // A button just right of cell 0 but well below it, and a far one in line.
    const std::vector<Target> targets{{{0.f, 0.f, 20.f, 20.f}}, {{25.f, 60.f, 45.f, 80.f}}, {{80.f, 0.f, 100.f, 20.f}}};
    CHECK(Pick::NextThatWay(targets, 0, 1, 0) == 2);
}

TEST_CASE("The d-pad leaves a window only with nothing more that way in it")
{
    std::vector<Target> targets = Grid(100.f, 0.f, 2, 1, 0);
    // Another window's button to the left, and one further right in it too.
    targets.push_back({{0.f, 0.f, 20.f, 20.f}, 1});
    targets.push_back({{200.f, 0.f, 220.f, 20.f}, 1});
    CHECK(Pick::NextThatWay(targets, 0, 1, 0) == 1);
    CHECK(Pick::NextThatWay(targets, 1, 1, 0) == 3);
    CHECK(Pick::NextThatWay(targets, 0, -1, 0) == 2);
}

TEST_CASE("No step from nowhere or without a way")
{
    const std::vector<Target> grid = Grid(0.f, 0.f, 2, 2);
    CHECK(Pick::NextThatWay(grid, Pick::None, 1, 0) == Pick::None);
    CHECK(Pick::NextThatWay(grid, 0, 0, 0) == Pick::None);
}

TEST_CASE("A window starts on its first cell, else its first target")
{
    // A close button above and right of a bag's grid: the grid's top-left cell comes first.
    std::vector<Target> targets{{{100.f, 0.f, 120.f, 10.f}}};
    for (Target cell : Grid(0.f, 20.f, 3, 2))
    {
        cell.cell = true;
        targets.push_back(cell);
    }
    CHECK(Pick::First(targets) == 1);
    // No cells: the one nearest the top-left corner, not a close button higher up on the right.
    const std::vector<Target> buttons{{{90.f, 0.f, 100.f, 10.f}}, {{10.f, 6.f, 30.f, 26.f}}, {{0.f, 40.f, 20.f, 60.f}}};
    CHECK(Pick::First(buttons) == 1);
    CHECK(Pick::First(std::vector<Target>{}) == Pick::None);
}
