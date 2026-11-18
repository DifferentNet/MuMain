#include "UI/Chat/WarpUnlocks.h"

#include <algorithm>

namespace UI::Chat::WarpUnlocks
{
namespace
{
constexpr int FullLevelRequirement = 400;
} // namespace

int RequiredLevel(int listedLevel, bool reducedClass)
{
    if (!reducedClass || listedLevel == FullLevelRequirement)
    {
        return listedLevel;
    }

    return static_cast<int>(static_cast<float>(listedLevel) * 2.f / 3.f);
}

std::vector<std::wstring> NewlyReached(const std::vector<Destination>& destinations, bool reducedClass, int oldLevel,
                                       int newLevel, std::size_t maxNames)
{
    std::vector<std::wstring> names;
    if (newLevel <= oldLevel)
    {
        return names;
    }

    const auto reachableBefore = [&](const std::wstring& name)
    {
        return std::any_of(destinations.begin(), destinations.end(), [&](const Destination& other)
                           { return other.name == name && RequiredLevel(other.listedLevel, reducedClass) <= oldLevel; });
    };

    for (const Destination& destination : destinations)
    {
        if (names.size() >= maxNames)
        {
            break;
        }

        const int required = RequiredLevel(destination.listedLevel, reducedClass);
        if (destination.name.empty() || required <= oldLevel || required > newLevel)
        {
            continue;
        }

        if (std::find(names.begin(), names.end(), destination.name) != names.end() || reachableBefore(destination.name))
        {
            continue;
        }

        names.push_back(destination.name);
    }

    return names;
}

std::wstring Join(const std::vector<std::wstring>& names)
{
    std::wstring joined;
    for (const std::wstring& name : names)
    {
        if (!joined.empty())
        {
            joined += L", ";
        }
        joined += name;
    }
    return joined;
}
} // namespace UI::Chat::WarpUnlocks
