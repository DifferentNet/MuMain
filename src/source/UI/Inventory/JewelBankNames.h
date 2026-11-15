#pragma once

#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Shorter jewel names for the jewel bank's phone view (UI/Inventory/JewelBankWindow.h), where the name column is
// narrow: "Jewel of Bless", "Jewel of Soul" and the rest share "Jewel of", which then only shrinks the text.
namespace UI::Inventory::JewelBankNames
{
// The names without the leading words most of them share: words (split on spaces only, so any UTF-8 or
// Cyrillic name stays whole) that more than half of the names, and at least two, start with. A name that
// doesn't start with them, or would have nothing left, stays as it is. Without such words the names come back
// unchanged.
std::vector<std::wstring> WithoutSharedLeadingWords(std::span<const std::wstring> names);

// A name broken onto two lines: the first and the second, without the space between them.
using TwoLines = std::pair<std::wstring, std::wstring>;

// The name broken at the space that makes the wider of its two lines narrowest, as `width` measures a line;
// every word is kept. A name without a space between two words gives an empty second line.
TwoLines SplitInTwo(const std::wstring& name, const std::function<float(const std::wstring&)>& width);
} // namespace UI::Inventory::JewelBankNames
