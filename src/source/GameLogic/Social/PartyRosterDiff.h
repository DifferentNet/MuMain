#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

// Who joined and who left between two party lists the server sent (ReceivePartyList), for the chat lines.
namespace GameLogic::Social::PartyRoster
{
struct Changes
{
    std::vector<std::wstring> joined;
    std::vector<std::wstring> left;
};

// Names in `after` and not in `before` joined, names in `before` and not in `after` left; `self` (the hero) is never
// named. An empty `before` gives no changes: that list is the hero's own join, a relog or a map change.
Changes Diff(std::span<const std::wstring> before, std::span<const std::wstring> after, std::wstring_view self);
} // namespace GameLogic::Social::PartyRoster
