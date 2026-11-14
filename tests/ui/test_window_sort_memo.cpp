#include <doctest.h>

#include "UI/Core/WindowSortMemo.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace
{
struct Window
{
    float depth;
    float keyOrder = 0.f;
};

bool Deeper(const Window* a, const Window* b)
{
    return a->depth < b->depth;
}

bool Shallower(const Window* a, const Window* b)
{
    return a->depth > b->depth;
}

bool KeysFirst(const Window* a, const Window* b)
{
    return a->keyOrder > b->keyOrder;
}

std::vector<float> DepthsOf(const std::vector<Window*>& windows)
{
    std::vector<float> depths;
    for (const Window* window : windows)
    {
        depths.push_back(window->depth);
    }
    return depths;
}

std::vector<float> KeyOrdersOf(const std::vector<Window*>& windows)
{
    std::vector<float> keys;
    for (const Window* window : windows)
    {
        keys.push_back(window->keyOrder);
    }
    return keys;
}

// Many equal depths and key orders, as in the game (about 105 windows on a few depths).
std::vector<Window> TiedWindows(int count = 40)
{
    std::vector<Window> windows;
    for (int i = 0; i < count; ++i)
    {
        windows.push_back({static_cast<float>((i * 7) % 4), static_cast<float>((i * 5) % 2)});
    }
    return windows;
}

std::vector<Window*> StableSorted(std::vector<Window*> items, bool (*compare)(const Window*, const Window*))
{
    std::stable_sort(items.begin(), items.end(), compare);
    return items;
}

std::vector<Window*> PointersTo(std::vector<Window>& windows)
{
    std::vector<Window*> pointers;
    for (Window& window : windows)
    {
        pointers.push_back(&window);
    }
    return pointers;
}
} // namespace

TEST_CASE("a remembered sort gives what std::stable_sort gives, ties included [ui][windows]")
{
    auto windows = TiedWindows();
    const auto input = PointersTo(windows);
    mu::ui::window::WindowSortMemo<Window*> memo;

    for (int frame = 0; frame < 3; ++frame)
    {
        auto items = input;
        memo.Sort(items, DepthsOf(items), false, Deeper);
        CHECK(items == StableSorted(input, Deeper));
    }
    CHECK(memo.SortsRun() == 1);
}

TEST_CASE("the manager's three orders settle by frame 2 and then never sort again [ui][windows]")
{
    for (const int count : {40, 105})
    {
        CAPTURE(count);
        auto windows = TiedWindows(count);
        auto items = PointersTo(windows);
        // The orders of one frame: mouse (depth, descending), keys (key order, descending), update (depth).
        mu::ui::window::WindowSortMemo<Window*> mouse, keys, update;

        constexpr int kSettledFrame = 2;
        std::size_t sortsBySettling = 0;
        for (int frame = 0; frame < kSettledFrame + 5; ++frame)
        {
            auto expected = StableSorted(items, Shallower);
            mouse.Sort(items, DepthsOf(items), true, Shallower);
            CHECK(items == expected);

            expected = StableSorted(items, KeysFirst);
            keys.Sort(items, KeyOrdersOf(items), true, KeysFirst);
            CHECK(items == expected);

            expected = StableSorted(items, Deeper);
            update.Sort(items, DepthsOf(items), false, Deeper);
            CHECK(items == expected);

            if (frame == kSettledFrame)
            {
                sortsBySettling = mouse.SortsRun() + keys.SortsRun() + update.SortsRun();
            }
        }
        CHECK(mouse.SortsRun() + keys.SortsRun() + update.SortsRun() == sortsBySettling);
    }
}

TEST_CASE("a changed depth or window list sorts again [ui][windows]")
{
    auto windows = TiedWindows();
    const auto input = PointersTo(windows);
    mu::ui::window::WindowSortMemo<Window*> memo;

    auto items = input;
    memo.Sort(items, DepthsOf(items), false, Deeper);

    windows[5].depth = 10.f;
    items = input;
    memo.Sort(items, DepthsOf(items), false, Deeper);
    CHECK(memo.SortsRun() == 2);
    CHECK(items.back() == &windows[5]);

    auto shorter = input;
    shorter.pop_back();
    const auto expected = StableSorted(shorter, Deeper);
    memo.Sort(shorter, DepthsOf(shorter), false, Deeper);
    CHECK(memo.SortsRun() == 3);
    CHECK(shorter == expected);
}

TEST_CASE("a list already in order is left as it is [ui][windows]")
{
    std::vector<Window> windows{{1.f}, {1.f}, {2.f}};
    auto items = PointersTo(windows);
    const auto before = items;
    mu::ui::window::WindowSortMemo<Window*> memo;

    memo.Sort(items, DepthsOf(items), false, Deeper);
    CHECK(items == before);
    CHECK(memo.SortsRun() == 0);
}
