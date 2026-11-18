#include "UI/HUD/MiniMapPartyDots.h"

namespace UI::MiniMap::PartyDots
{
std::vector<Dot> OnMap(const std::vector<Member>& members, int map, const std::wstring& heroName)
{
    std::vector<Dot> dots;
    for (const Member& member : members)
    {
        if (member.name.empty() || member.map != map || member.name == heroName)
            continue;
        dots.push_back({member.name, member.x, member.y});
    }
    return dots;
}
} // namespace UI::MiniMap::PartyDots
