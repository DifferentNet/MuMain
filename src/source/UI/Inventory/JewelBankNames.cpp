#include "UI/Inventory/JewelBankNames.h"

#include <algorithm>
#include <cstddef>
#include <map>

namespace
{
constexpr wchar_t kWordSeparator = L' ';
// Words shared by at least this many names (and by more than half of them) are dropped.
constexpr std::size_t kMinimumSharingNames = 2;

// The first `words` words of the name with the space after them, or empty when the name has no more words than
// that (so a word stays after the prefix).
std::wstring LeadingWords(const std::wstring& name, std::size_t words)
{
    std::size_t end = 0;
    for (std::size_t word = 0; word < words; ++word)
    {
        const std::size_t space = name.find(kWordSeparator, end);
        if (space == std::wstring::npos)
            return {};
        end = space + 1;
    }
    if (end >= name.size())
        return {};
    return name.substr(0, end);
}

bool IsShared(std::size_t sharing, std::size_t total)
{
    return sharing >= kMinimumSharingNames && sharing * 2 > total;
}

// The longest run of leading words that more than half of the names share, or empty.
std::wstring SharedLeadingWords(std::span<const std::wstring> names)
{
    std::wstring shared;
    for (std::size_t words = 1;; ++words)
    {
        std::map<std::wstring, std::size_t> counts;
        for (const std::wstring& name : names)
        {
            std::wstring prefix = LeadingWords(name, words);
            if (!prefix.empty() && prefix.starts_with(shared))
                ++counts[std::move(prefix)];
        }
        const std::pair<const std::wstring, std::size_t>* best = nullptr;
        for (const auto& entry : counts)
        {
            if (best == nullptr || entry.second > best->second)
                best = &entry;
        }
        if (best == nullptr || !IsShared(best->second, names.size()))
            return shared;
        shared = best->first;
    }
}
} // namespace

std::vector<std::wstring> UI::Inventory::JewelBankNames::WithoutSharedLeadingWords(std::span<const std::wstring> names)
{
    std::vector<std::wstring> result(names.begin(), names.end());
    const std::wstring shared = SharedLeadingWords(names);
    if (shared.empty())
        return result;
    for (std::wstring& name : result)
    {
        if (name.size() > shared.size() && name.starts_with(shared))
            name.erase(0, shared.size());
    }
    return result;
}

UI::Inventory::JewelBankNames::TwoLines
UI::Inventory::JewelBankNames::SplitInTwo(const std::wstring& name,
                                          const std::function<float(const std::wstring&)>& width)
{
    TwoLines best{name, {}};
    float bestWidth = 0.f;
    for (std::size_t space = name.find(kWordSeparator); space != std::wstring::npos;
         space = name.find(kWordSeparator, space + 1))
    {
        if (space == 0 || space + 1 >= name.size())
            continue;
        TwoLines lines{name.substr(0, space), name.substr(space + 1)};
        const float wider = std::max(width(lines.first), width(lines.second));
        if (best.second.empty() || wider < bestWidth)
        {
            best = std::move(lines);
            bestWidth = wider;
        }
    }
    return best;
}
