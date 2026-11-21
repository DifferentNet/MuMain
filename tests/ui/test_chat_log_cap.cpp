#include "doctest.h"

#include "UI/Chat/ChatLogCap.h"

#include <deque>

using UI::Chat::LinesDroppedAtCap;

namespace
{
constexpr int Most = 200;
constexpr int Dropped = 20;

// A chat list as CChatLogWindow keeps it: rows added at the end, the oldest dropped at the cap, a message that can't
// be measured yet waiting until it can (AddText(), AddPendingMessages(), AddRows()).
struct ChatList
{
    int lines = 0;
    int droppedInAll = 0;
    std::deque<int> waiting; // rows of each waiting message

    void AddRows(int rows)
    {
        const int dropped = LinesDroppedAtCap(lines, Most, Dropped);
        lines -= dropped;
        droppedInAll += dropped;
        lines += rows;
    }
    void Wait(int rows) { waiting.push_back(rows); }
    void Flush()
    {
        while (!waiting.empty())
        {
            AddRows(waiting.front());
            waiting.pop_front();
        }
    }
};
} // namespace

TEST_CASE("A full chat list drops its oldest lines at once")
{
    CHECK(LinesDroppedAtCap(0, Most, Dropped) == 0);
    CHECK(LinesDroppedAtCap(Most - 1, Most, Dropped) == 0);
    CHECK(LinesDroppedAtCap(Most, Most, Dropped) == Dropped);
    // A message of several rows may take the list past the cap; the next one still drops the same.
    CHECK(LinesDroppedAtCap(Most + 3, Most, Dropped) == Dropped);
}

TEST_CASE("200 lines and a waiting message: 20 dropped once, when it is added")
{
    ChatList list;
    list.lines = Most;
    list.Wait(1);
    CHECK(list.droppedInAll == 0);
    CHECK(list.lines == Most);

    list.Flush();
    CHECK(list.droppedInAll == Dropped);
    CHECK(list.lines == Most - Dropped + 1);

    // Nothing more goes until the list is full again.
    list.AddRows(1);
    CHECK(list.droppedInAll == Dropped);
}
