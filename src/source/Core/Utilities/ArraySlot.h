#pragma once

#include <cstdint>

namespace Core::Utilities
{
// The index of the array element whose member `member` lies at `address`, found by address arithmetic instead of a
// loop over the elements; -1 when the address is outside the array or not at that member of an element. Gives the
// same slot as comparing &items[i].*member with the address for every i.
template <typename T, typename M>
int SlotOfMember(const T* items, int count, M T::*member, const void* address)
{
    if (items == nullptr || address == nullptr || count <= 0)
        return -1;

    const auto first = reinterpret_cast<std::uintptr_t>(&(items[0].*member));
    const auto at = reinterpret_cast<std::uintptr_t>(address);
    if (at < first)
        return -1;
    const std::uintptr_t distance = at - first;
    if (distance % sizeof(T) != 0)
        return -1;
    const std::uintptr_t index = distance / sizeof(T);
    return index < static_cast<std::uintptr_t>(count) ? static_cast<int>(index) : -1;
}
} // namespace Core::Utilities
