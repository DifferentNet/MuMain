#include "GameLogic/Social/PartyRosterDiff.h"

#include <algorithm>

namespace GameLogic::Social::PartyRoster
{
namespace
{
bool Contains(std::span<const std::wstring> names, const std::wstring& name)
{
    return std::find(names.begin(), names.end(), name) != names.end();
}

// The names of `from` missing in `other`, without `self` and without empty names.
std::vector<std::wstring> Missing(std::span<const std::wstring> from, std::span<const std::wstring> other,
                                  std::wstring_view self)
{
    std::vector<std::wstring> result;
    for (const std::wstring& name : from)
    {
        if (!name.empty() && name != self && !Contains(other, name))
        {
            result.push_back(name);
        }
    }
    return result;
}
} // namespace

Changes Diff(std::span<const std::wstring> before, std::span<const std::wstring> after, std::wstring_view self)
{
    if (before.empty())
    {
        return {};
    }
    return Changes{ Missing(after, before, self), Missing(before, after, self) };
}
} // namespace GameLogic::Social::PartyRoster
