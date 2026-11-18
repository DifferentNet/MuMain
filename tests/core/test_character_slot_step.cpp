#include <doctest.h>

#include "Scenes/CharacterSlotStep.h"

#include <array>

using Scenes::CharacterSelect::StepToFilledSlot;

TEST_CASE("character select steps to the next filled slot and wraps around")
{
    const std::array<bool, 5> slots{true, false, true, true, false};
    const auto filled = [&](int slot) { return slots[static_cast<std::size_t>(slot)]; };

    CHECK(StepToFilledSlot(0, 1, 5, filled) == 2);
    CHECK(StepToFilledSlot(2, 1, 5, filled) == 3);
    CHECK(StepToFilledSlot(3, 1, 5, filled) == 0);
    CHECK(StepToFilledSlot(0, -1, 5, filled) == 3);
    CHECK(StepToFilledSlot(2, -1, 5, filled) == 0);
}

TEST_CASE("character select from no selection picks the first or the last character")
{
    const std::array<bool, 5> slots{false, true, false, true, false};
    const auto filled = [&](int slot) { return slots[static_cast<std::size_t>(slot)]; };

    CHECK(StepToFilledSlot(-1, 1, 5, filled) == 1);
    CHECK(StepToFilledSlot(-1, -1, 5, filled) == 3);
}

TEST_CASE("character select stays put with one character or none")
{
    const std::array<bool, 5> one{false, false, true, false, false};
    const auto oneFilled = [&](int slot) { return one[static_cast<std::size_t>(slot)]; };
    CHECK(StepToFilledSlot(2, 1, 5, oneFilled) == 2);
    CHECK(StepToFilledSlot(2, -1, 5, oneFilled) == 2);

    const auto noneFilled = [](int) { return false; };
    CHECK(StepToFilledSlot(-1, 1, 5, noneFilled) == -1);
}
