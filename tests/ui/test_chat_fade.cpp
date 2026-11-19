#include <doctest.h>

#include "UI/Chat/ChatFade.h"

using UI::Chat::ChatLineShownMs;
using UI::Chat::EvaluateChatFade;
using UI::Chat::IsChatLineShown;
using UI::Chat::SystemLinesShown;
using UI::Chat::FoldsIntoNewestSystemLine;
using UI::Chat::SystemLineRepeatMs;
using UI::Chat::WithRepeatCount;

namespace
{
// Hours into the clock, so every "minutes ago" below stays above zero.
constexpr std::uint64_t Now = 10ull * 60 * 60'000;
} // namespace

TEST_CASE("Closed chat shows only the lines younger than the limit")
{
    const auto state = EvaluateChatFade({Now - 25 * 60'000, Now - 31'000, Now - 10'000, Now}, Now, false);

    CHECK(state.shown == std::vector<bool>{false, false, true, true});
    CHECK(state.firstShown == 2);
}

TEST_CASE("Open chat shows every line and nothing is due")
{
    const auto state = EvaluateChatFade({Now - 25 * 60'000, Now - 31'000, Now}, Now, true);

    CHECK(state.shown == std::vector<bool>{true, true, true});
    CHECK(state.firstShown == 0);
    CHECK_FALSE(state.nextChangeMs.has_value());
}

TEST_CASE("A line goes exactly at the limit")
{
    CHECK(IsChatLineShown(Now - ChatLineShownMs + 1, Now, false));
    CHECK_FALSE(IsChatLineShown(Now - ChatLineShownMs, Now, false));
    CHECK(IsChatLineShown(Now - ChatLineShownMs, Now, true));
}

TEST_CASE("A line stamped after now stays")
{
    CHECK(IsChatLineShown(Now + 5, Now, false));
}

TEST_CASE("No lines")
{
    const auto closed = EvaluateChatFade({}, Now, false);
    CHECK(closed.shown.empty());
    CHECK(closed.firstShown == -1);
    CHECK_FALSE(closed.nextChangeMs.has_value());

    const auto open = EvaluateChatFade({}, Now, true);
    CHECK(open.shown.empty());
    CHECK(open.firstShown == -1);
    CHECK_FALSE(open.nextChangeMs.has_value());
}

TEST_CASE("Next change is when the oldest line on screen goes")
{
    const auto state = EvaluateChatFade({Now - 40'000, Now - 20'000, Now - 5'000}, Now, false);

    REQUIRE(state.nextChangeMs.has_value());
    CHECK(*state.nextChangeMs == Now - 20'000 + ChatLineShownMs);
}

TEST_CASE("Next change ignores line order")
{
    const auto state = EvaluateChatFade({Now - 5'000, Now - 20'000}, Now, false);

    REQUIRE(state.nextChangeMs.has_value());
    CHECK(*state.nextChangeMs == Now - 20'000 + ChatLineShownMs);
}

TEST_CASE("Nothing is due once every line has gone")
{
    const auto state = EvaluateChatFade({Now - 40'000, Now - ChatLineShownMs}, Now, false);

    CHECK(state.shown == std::vector<bool>{false, false});
    CHECK(state.firstShown == -1);
    CHECK_FALSE(state.nextChangeMs.has_value());
}

TEST_CASE("System log: only the newest lines younger than the stay show")
{
    const std::vector<std::uint64_t> arrivals{Now - 20 * 60'000, Now - 31'000, Now - 30'000, Now - 29'999, Now};
    CHECK(SystemLinesShown(arrivals, Now, 6, 30'000, false) == 2);
}

TEST_CASE("System log: the line cap holds even for fresh lines")
{
    const std::vector<std::uint64_t> arrivals{Now - 3, Now - 2, Now - 1, Now};
    CHECK(SystemLinesShown(arrivals, Now, 3, 8'000, false) == 3);
}

TEST_CASE("System log: an open chat shows the newest lines whatever their age")
{
    const std::vector<std::uint64_t> arrivals{Now - 20 * 60'000, Now - 10 * 60'000, Now - 60'000};
    CHECK(SystemLinesShown(arrivals, Now, 6, 30'000, true) == 3);
    CHECK(SystemLinesShown(arrivals, Now, 2, 30'000, true) == 2);
    CHECK(SystemLinesShown(arrivals, Now, 6, 30'000, false) == 0);
}

TEST_CASE("System log: nothing to show and a clock behind an arrival")
{
    CHECK(SystemLinesShown({}, Now, 6, 30'000, false) == 0);
    CHECK(SystemLinesShown({Now + 5}, Now, 6, 30'000, false) == 1);
}

TEST_CASE("System log: the same message within 3 s folds into the newest line")
{
    CHECK(FoldsIntoNewestSystemLine(true, Now - 1, Now));
    CHECK(FoldsIntoNewestSystemLine(true, Now - SystemLineRepeatMs + 1, Now));
    CHECK_FALSE(FoldsIntoNewestSystemLine(true, Now - SystemLineRepeatMs, Now));
    CHECK_FALSE(FoldsIntoNewestSystemLine(false, Now - 1, Now));
    CHECK(FoldsIntoNewestSystemLine(true, Now + 5, Now));
}

TEST_CASE("System log: a repeated line shows its count")
{
    CHECK(WithRepeatCount(L"No arrows", 1) == L"No arrows");
    CHECK(WithRepeatCount(L"No arrows", 0) == L"No arrows");
    CHECK(WithRepeatCount(L"No arrows", 3) == L"No arrows \u00D73");
}
