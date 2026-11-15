#include "UI/HUD/PadButtonNames.h"

#include <array>
#include <utility>

namespace UI::HUD::PadButtonNames
{
std::wstring Fill(std::wstring_view text, const Names& names)
{
    const std::array<std::pair<std::wstring_view, const std::wstring*>, 12> places = {{
        {L"A", &names.a},
        {L"B", &names.b},
        {L"X", &names.x},
        {L"Y", &names.y},
        {L"LB", &names.lb},
        {L"RB", &names.rb},
        {L"LT", &names.lt},
        {L"RT", &names.rt},
        {L"LS", &names.ls},
        {L"RS", &names.rs},
        {L"MENU", &names.menu},
        {L"VIEW", &names.view},
    }};
    std::wstring filled;
    filled.reserve(text.size());
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t open = text.find(L'{', at);
        if (open == std::wstring_view::npos)
            break;
        const std::size_t close = text.find(L'}', open + 1);
        if (close == std::wstring_view::npos)
            break;
        const std::wstring_view key = text.substr(open + 1, close - open - 1);
        const std::wstring* name = nullptr;
        for (const auto& [placeKey, placeName] : places)
        {
            if (placeKey == key)
                name = placeName;
        }
        filled.append(text.substr(at, open - at));
        if (name != nullptr)
        {
            filled.append(*name);
            at = close + 1;
        }
        else
        {
            // Not a button: the brace stays, and the search goes on after it.
            filled.push_back(L'{');
            at = open + 1;
        }
    }
    filled.append(text.substr(at));
    return filled;
}
} // namespace UI::HUD::PadButtonNames
