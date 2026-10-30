#include <doctest.h>

#include "UI/Mobile/SkillArrange.h"

namespace SkillArrange = UI::Mobile::SkillArrange;

namespace
{
using SkillArrange::HotKeys;
using SkillArrange::NoSkill;

// Skills as the list numbers them, and a pet command (AT_PET_COMMAND_DEFAULT is 120).
constexpr int FireBall = 3;
constexpr int Meteor = 5;
constexpr int Teleport = 7;
constexpr int PetAttack = 121;

HotKeys Empty()
{
    HotKeys keys{};
    keys.fill(NoSkill);
    return keys;
}
} // namespace

TEST_CASE("the buttons stand for hotkeys 1-5, or 6-9 and 0 on the upper set [ui][skill-arrange]")
{
    CHECK(SkillArrange::HotKeyOfSlot(0, false) == 1);
    CHECK(SkillArrange::HotKeyOfSlot(4, false) == 5);
    CHECK(SkillArrange::HotKeyOfSlot(0, true) == 6);
    CHECK(SkillArrange::HotKeyOfSlot(3, true) == 9);
    CHECK(SkillArrange::HotKeyOfSlot(4, true) == 0);
}

TEST_CASE("a skill on no button goes on the chosen one, and what it held comes off [ui][skill-arrange]")
{
    HotKeys keys = Empty();
    keys[2] = Meteor;
    const HotKeys placed = SkillArrange::Placed(keys, 2, FireBall);
    CHECK(placed[2] == FireBall);
    CHECK(SkillArrange::HotKeyOf(placed, Meteor) == -1);

    const HotKeys onEmpty = SkillArrange::Placed(Empty(), 1, Teleport);
    CHECK(onEmpty[1] == Teleport);
    CHECK(SkillArrange::HotKeyOf(onEmpty, Teleport) == 1);
}

TEST_CASE("a skill on another button swaps with what the chosen one held [ui][skill-arrange]")
{
    HotKeys keys = Empty();
    keys[1] = FireBall;
    keys[2] = Meteor;
    const HotKeys swapped = SkillArrange::Placed(keys, 1, Meteor);
    CHECK(swapped[1] == Meteor);
    CHECK(swapped[2] == FireBall);

    // Onto an empty button it moves: its old button empties.
    const HotKeys moved = SkillArrange::Placed(keys, 4, FireBall);
    CHECK(moved[4] == FireBall);
    CHECK(moved[1] == NoSkill);
    CHECK(moved[2] == Meteor);

    // A hotkey of the set the bar does not show swaps the same way.
    keys[7] = PetAttack;
    const HotKeys fromUpper = SkillArrange::Placed(keys, 1, PetAttack);
    CHECK(fromUpper[1] == PetAttack);
    CHECK(fromUpper[7] == FireBall);
}

TEST_CASE("a skill put where it is already, or out of range, changes nothing [ui][skill-arrange]")
{
    HotKeys keys = Empty();
    keys[3] = FireBall;
    CHECK(SkillArrange::Placed(keys, 3, FireBall) == keys);
    CHECK(SkillArrange::Placed(keys, -1, Meteor) == keys);
    CHECK(SkillArrange::Placed(keys, SkillArrange::HotKeyCount, Meteor) == keys);
}

TEST_CASE("no skill empties a button and touches no other [ui][skill-arrange]")
{
    HotKeys keys = Empty();
    keys[3] = FireBall;
    keys[4] = Meteor;
    const HotKeys cleared = SkillArrange::Placed(keys, 3, NoSkill);
    CHECK(cleared[3] == NoSkill);
    CHECK(cleared[4] == Meteor);
    CHECK(SkillArrange::HotKeyOf(cleared, NoSkill) == -1);
}

TEST_CASE("placing keeps every skill on one hotkey at most [ui][skill-arrange]")
{
    const int skills[] = {FireBall, Meteor, Teleport, PetAttack, NoSkill};
    HotKeys keys = Empty();
    int step = 0;
    for (int round = 0; round < 40; ++round)
    {
        for (const int skill : skills)
        {
            keys = SkillArrange::Placed(keys, (step * 7 + round) % SkillArrange::HotKeyCount, skill);
            ++step;
            for (const int other : skills)
            {
                if (other == NoSkill)
                    continue;
                int holders = 0;
                for (const int held : keys)
                    holders += held == other ? 1 : 0;
                CHECK(holders <= 1);
            }
        }
    }
}

TEST_CASE("the next empty button is found round the five, the current one when all are taken [ui][skill-arrange]")
{
    HotKeys keys = Empty();
    keys[1] = FireBall;
    keys[2] = Meteor;
    CHECK(SkillArrange::NextEmptySlot(keys, false, 0) == 2);
    keys[3] = Teleport;
    keys[4] = PetAttack;
    CHECK(SkillArrange::NextEmptySlot(keys, false, 3) == 4);
    CHECK(SkillArrange::NextEmptySlot(keys, false, 4) == 4);
    keys[5] = 9;
    CHECK(SkillArrange::NextEmptySlot(keys, false, 2) == 2);
    // The upper set looks at hotkeys 6-9 and 0.
    CHECK(SkillArrange::NextEmptySlot(keys, true, 0) == 1);
}

TEST_CASE("a page holds the rows and columns that fit, at least one entry [ui][skill-arrange]")
{
    // The phone's grid: 468 x 214 units, entries 104 x 50 with 6 between.
    CHECK(SkillArrange::PageCapacity(468.f, 214.f, 104.f, 50.f, 6.f) == 4 * 3);
    // Exactly four columns: 4 x 104 + 3 x 6 = 434.
    CHECK(SkillArrange::PageCapacity(434.f, 50.f, 104.f, 50.f, 6.f) == 4);
    CHECK(SkillArrange::PageCapacity(433.f, 50.f, 104.f, 50.f, 6.f) == 3);
    CHECK(SkillArrange::PageCapacity(10.f, 10.f, 104.f, 50.f, 6.f) == 1);
    CHECK(SkillArrange::PageCapacity(500.f, 500.f, 0.f, 50.f, 6.f) == 1);
}

TEST_CASE("the pages cover every entry, one at least [ui][skill-arrange]")
{
    CHECK(SkillArrange::PageCount(0, 12) == 1);
    CHECK(SkillArrange::PageCount(12, 12) == 1);
    CHECK(SkillArrange::PageCount(13, 12) == 2);
    CHECK(SkillArrange::PageCount(25, 12) == 3);
    CHECK(SkillArrange::PageCount(5, 0) == 1);
}
