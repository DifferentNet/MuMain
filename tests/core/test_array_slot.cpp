#include "doctest.h"

#include "Core/Utilities/ArraySlot.h"

#include <array>

using Core::Utilities::SlotOfMember;

namespace
{
struct Inner
{
    int value = 0;
};

struct Element
{
    double before = 0;
    Inner member;
    char after[13] = {};
};

constexpr int Count = 400;
std::array<Element, Count> g_items;

// The loop SlotOfMember replaces.
int LoopSlot(const void* address)
{
    for (int i = 0; i < Count; ++i)
    {
        if (&g_items[static_cast<std::size_t>(i)].member == address)
            return i;
    }
    return -1;
}
} // namespace

TEST_CASE("SlotOfMember finds the slot of a member like the loop")
{
    for (const int slot : {0, 1, 200, Count - 1})
    {
        const Inner* address = &g_items[static_cast<std::size_t>(slot)].member;
        CHECK(SlotOfMember(g_items.data(), Count, &Element::member, address) == slot);
        CHECK(LoopSlot(address) == slot);
    }
}

TEST_CASE("SlotOfMember rejects addresses that are not a member of an element")
{
    const Inner outside;
    CHECK(SlotOfMember(g_items.data(), Count, &Element::member, &outside) == -1);
    CHECK(SlotOfMember(g_items.data(), Count, &Element::member, &g_items[3].before) == -1);
    CHECK(SlotOfMember(g_items.data(), Count, &Element::member, &g_items[3].after[1]) == -1);
    CHECK(SlotOfMember(g_items.data(), Count, &Element::member, nullptr) == -1);
    const char* pastLast = reinterpret_cast<const char*>(&g_items[Count - 1].member) + sizeof(Element);
    CHECK(SlotOfMember(g_items.data(), Count, &Element::member, pastLast) == -1);
    CHECK(SlotOfMember<Element, Inner>(nullptr, Count, &Element::member, &g_items[0].member) == -1);
    CHECK(SlotOfMember(g_items.data(), 0, &Element::member, &g_items[0].member) == -1);
}
