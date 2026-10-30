#include <doctest.h>

#include "UI/NPCs/ShopTapToBuy.h"

#include <chrono>

namespace TapToBuy = UI::Shop::TapToBuy;

namespace
{
using TapToBuy::Action;
using std::chrono::milliseconds;

constexpr std::uint32_t Sword = 7;
constexpr std::uint32_t Shield = 9;
const TapToBuy::Clock::time_point Start{};
} // namespace

TEST_CASE("a first tap on a shop item chooses it and buys nothing [ui][shop]")
{
    TapToBuy::Choice choice;
    CHECK(TapToBuy::Tap(choice, Sword, Start) == Action::Choose);
    CHECK(TapToBuy::Chosen(choice, Start) == Sword);
}

TEST_CASE("a second tap on the chosen item soon after buys it, and the choice is gone [ui][shop]")
{
    TapToBuy::Choice choice;
    (void)TapToBuy::Tap(choice, Sword, Start);
    CHECK(TapToBuy::Tap(choice, Sword, Start + TapToBuy::SecondTapTime) == Action::Buy);
    CHECK_FALSE(TapToBuy::Chosen(choice, Start + TapToBuy::SecondTapTime).has_value());
    // A third tap chooses again: one more pair of taps for one more of it.
    CHECK(TapToBuy::Tap(choice, Sword, Start + TapToBuy::SecondTapTime + milliseconds(100)) == Action::Choose);
}

TEST_CASE("a tap on another item chooses that one instead [ui][shop]")
{
    TapToBuy::Choice choice;
    (void)TapToBuy::Tap(choice, Sword, Start);
    CHECK(TapToBuy::Tap(choice, Shield, Start + milliseconds(300)) == Action::Choose);
    CHECK(TapToBuy::Chosen(choice, Start + milliseconds(300)) == Shield);
    CHECK(TapToBuy::Tap(choice, Shield, Start + milliseconds(600)) == Action::Buy);
}

TEST_CASE("a second tap too late only chooses the item again, from then on [ui][shop]")
{
    TapToBuy::Choice choice;
    (void)TapToBuy::Tap(choice, Sword, Start);
    const auto late = Start + TapToBuy::SecondTapTime + milliseconds(1);
    CHECK_FALSE(TapToBuy::Chosen(choice, late).has_value());
    CHECK(TapToBuy::Tap(choice, Sword, late) == Action::Choose);
    CHECK(TapToBuy::Tap(choice, Sword, late + milliseconds(500)) == Action::Buy);
}
