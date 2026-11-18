#include <doctest.h>

#include "UI/HUD/MiniMapPartyDots.h"
#include "UI/Party/PartyListRefresh.h"

using UI::MiniMap::PartyDots::Dot;
using UI::MiniMap::PartyDots::Member;
using UI::MiniMap::PartyDots::OnMap;
using UI::Party::ListRefresh::RefreshDue;

TEST_CASE("party dots: members on the shown map, without the hero [ui][minimap]")
{
    const std::vector<Member> members = {
        {L"Hero", 0, 130, 120},
        {L"Alice", 0, 140, 125},
        {L"Bob", 2, 10, 20},
        {L"", 0, 1, 1},
        {L"Carol", 0, 200, 50},
    };
    const auto dots = OnMap(members, 0, L"Hero");
    REQUIRE(dots.size() == 2);
    CHECK(dots[0] == Dot{L"Alice", 140, 125});
    CHECK(dots[1] == Dot{L"Carol", 200, 50});

    CHECK(OnMap(members, 2, L"Hero") == std::vector<Dot>{{L"Bob", 10, 20}});
    CHECK(OnMap(members, 7, L"Hero").empty());
    CHECK(OnMap({}, 0, L"Hero").empty());
}

TEST_CASE("party list refresh: every 5 seconds only while wanted [ui][minimap]")
{
    CHECK_FALSE(RefreshDue(false, false, 0, 10'000, 5'000));
    CHECK(RefreshDue(true, false, 0, 0, 5'000));
    CHECK_FALSE(RefreshDue(true, true, 1'000, 5'999, 5'000));
    CHECK(RefreshDue(true, true, 1'000, 6'000, 5'000));
    CHECK_FALSE(RefreshDue(false, true, 1'000, 60'000, 5'000));
    // The tick counter wraps.
    CHECK(RefreshDue(true, true, 0xFFFF'F000u, 0x0000'1000u, 5'000));
    CHECK_FALSE(RefreshDue(true, true, 0xFFFF'F000u, 0x0000'0100u, 5'000));
}
