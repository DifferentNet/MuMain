#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// Reordering recorded draws to cut state changes. Draws whose result does not depend on their order
// (opaque, depth-tested and depth-writing) move ahead of the rest and are grouped by state; the
// others keep their recorded order after them, which also draws them over what is opaque, as
// transparency should.
namespace Render
{

// Puts the `reorderable` commands first, stable-sorted by `less`, then the others in their order.
// Sorts indices and then moves each command once: the renderer's commands are a few hundred bytes,
// and moving them inside the sort cost more than comparing them (2026-11-03).
template <typename Command, typename Reorderable, typename Less>
void GroupReorderableFirst(std::span<Command> commands, Reorderable reorderable, Less less)
{
    static std::vector<std::uint32_t> order;
    static std::vector<Command> arranged;
    order.clear();
    for (std::uint32_t i = 0; i < commands.size(); ++i)
    {
        if (reorderable(commands[i]))
            order.push_back(i);
    }
    if (order.empty())
        return;
    std::stable_sort(order.begin(), order.end(),
                     [&](std::uint32_t left, std::uint32_t right) { return less(commands[left], commands[right]); });
    for (std::uint32_t i = 0; i < commands.size(); ++i)
    {
        if (!reorderable(commands[i]))
            order.push_back(i);
    }
    arranged.clear();
    arranged.reserve(commands.size());
    for (const std::uint32_t index : order)
        arranged.push_back(std::move(commands[index]));
    std::move(arranged.begin(), arranged.end(), commands.begin());
}

} // namespace Render
