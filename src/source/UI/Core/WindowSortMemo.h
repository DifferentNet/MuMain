#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace mu::ui::window
{
// Sorts one window order with std::stable_sort and remembers the last sort. The manager sorts the same list by up
// to three orders a frame, each undoing the last; stable sorts keep equal windows in their order, so after the
// first frames the same input comes back every frame and gets the remembered result instead of another sort.
// The result is always what std::stable_sort(items, compare) gives on that input.
template <typename T>
class WindowSortMemo
{
public:
    // `keys[i]` is the key of `items[i]` that `compare` orders by: ascending, or descending when `descending`.
    template <typename Compare>
    void Sort(std::vector<T>& items, std::span<const float> keys, bool descending, Compare compare)
    {
        assert(keys.size() == items.size());
        const auto inOrder = [descending](float a, float b) { return descending ? a > b : a < b; };
        if (std::is_sorted(keys.begin(), keys.end(), inOrder))
        {
            return;
        }
        if (m_remembered && items == m_input && std::equal(keys.begin(), keys.end(), m_keys.begin(), m_keys.end()))
        {
            items = m_result;
            assert(MatchesFreshSort(compare));
            return;
        }

        m_input = items;
        m_keys.assign(keys.begin(), keys.end());
        std::stable_sort(items.begin(), items.end(), compare);
        m_result = items;
        m_remembered = true;
        ++m_sortsRun;
    }

    // How many times Sort() really sorted (for tests).
    [[nodiscard]] std::size_t SortsRun() const { return m_sortsRun; }

private:
    template <typename Compare>
    [[nodiscard]] bool MatchesFreshSort(Compare compare) const
    {
        std::vector<T> fresh = m_input;
        std::stable_sort(fresh.begin(), fresh.end(), compare);
        return fresh == m_result;
    }

    std::vector<T> m_input;
    std::vector<float> m_keys;
    std::vector<T> m_result;
    bool m_remembered = false;
    std::size_t m_sortsRun = 0;
};
} // namespace mu::ui::window
