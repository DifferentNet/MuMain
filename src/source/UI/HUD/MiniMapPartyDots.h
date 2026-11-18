#pragma once

#include <string>
#include <vector>

// The party members the big map (Tab) marks: those on the map shown, without the hero. The party list carries each
// member's map and tile (it is asked again every few seconds while the map or the party window is open,
// UI/Party/PartyListRefresh.h).
namespace UI::MiniMap::PartyDots
{
struct Member
{
    std::wstring name;
    int map = -1;
    int x = 0;
    int y = 0;
};

struct Dot
{
    std::wstring name;
    int x = 0;
    int y = 0;

    bool operator==(const Dot&) const = default;
};

// The dots for `map`, in the party's order; members on other maps, without a name and the hero are left out.
[[nodiscard]] std::vector<Dot> OnMap(const std::vector<Member>& members, int map, const std::wstring& heroName);
} // namespace UI::MiniMap::PartyDots
