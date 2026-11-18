#pragma once

// Which warp destinations (the M list) a level-up has just opened, for one line in the chat.
#include <cstddef>
#include <string>
#include <vector>

namespace UI::Chat::WarpUnlocks
{
struct Destination
{
    std::wstring name;
    // The level the list asks (before the class rule).
    int listedLevel = 0;
};

// The level a destination asks of a class: the Dark classes (Magic Gladiator, Dark Lord, Rage Fighter) need two
// thirds of the listed level, except at 400.
[[nodiscard]] int RequiredLevel(int listedLevel, bool reducedClass);

// The names that became reachable between `oldLevel` (exclusive) and `newLevel` (inclusive), in list order, each
// once. A name already reachable at `oldLevel` (another floor of the same map) is left out. At most `maxNames`.
[[nodiscard]] std::vector<std::wstring> NewlyReached(const std::vector<Destination>& destinations, bool reducedClass,
                                                     int oldLevel, int newLevel, std::size_t maxNames = 2);

// The names joined with ", ".
[[nodiscard]] std::wstring Join(const std::vector<std::wstring>& names);
} // namespace UI::Chat::WarpUnlocks
