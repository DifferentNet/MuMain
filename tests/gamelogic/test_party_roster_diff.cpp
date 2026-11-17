#include <doctest.h>

#include "GameLogic/Social/PartyRosterDiff.h"

#include <string>
#include <vector>

using GameLogic::Social::PartyRoster::Diff;

TEST_CASE("a new name joined and a missing name left")
{
    const std::vector<std::wstring> before{ L"Hero", L"Alice", L"Bob" };
    const std::vector<std::wstring> after{ L"Hero", L"Bob", L"Carol" };

    const auto changes = Diff(before, after, L"Hero");

    REQUIRE(changes.joined.size() == 1);
    CHECK(changes.joined[0] == L"Carol");
    REQUIRE(changes.left.size() == 1);
    CHECK(changes.left[0] == L"Alice");
}

TEST_CASE("the first list after joining, a relog or a map change names nobody")
{
    const std::vector<std::wstring> before;
    const std::vector<std::wstring> after{ L"Alice", L"Hero" };

    const auto changes = Diff(before, after, L"Hero");

    CHECK(changes.joined.empty());
    CHECK(changes.left.empty());
}

TEST_CASE("the same names again in another order name nobody")
{
    const std::vector<std::wstring> before{ L"Hero", L"Alice", L"Bob" };
    const std::vector<std::wstring> after{ L"Bob", L"Hero", L"Alice" };

    const auto changes = Diff(before, after, L"Hero");

    CHECK(changes.joined.empty());
    CHECK(changes.left.empty());
}

TEST_CASE("the hero is never named and an emptied list names everyone else as left")
{
    const std::vector<std::wstring> before{ L"Hero", L"Alice" };
    const std::vector<std::wstring> after;

    const auto changes = Diff(before, after, L"Hero");

    CHECK(changes.joined.empty());
    REQUIRE(changes.left.size() == 1);
    CHECK(changes.left[0] == L"Alice");
}
