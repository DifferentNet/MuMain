#include <doctest.h>

#include "UI/Chat/ChatHistory.h"

using UI::Chat::ChatHistory;

TEST_CASE("Up starts at the newest line and stops at the oldest")
{
    ChatHistory history;
    history.Add(L"one");
    history.Add(L"two");
    history.Add(L"three");

    CHECK(history.Older(L"") == std::wstring(L"three"));
    CHECK(history.Older(L"") == std::wstring(L"two"));
    CHECK(history.Older(L"") == std::wstring(L"one"));
    CHECK(history.Older(L"") == std::wstring(L"one"));
}

TEST_CASE("Down walks back and returns the draft past the newest line")
{
    ChatHistory history;
    history.Add(L"one");
    history.Add(L"two");

    CHECK(history.Older(L"half typed") == std::wstring(L"two"));
    CHECK(history.Older(L"ignored") == std::wstring(L"one"));
    CHECK(history.Newer() == std::wstring(L"two"));
    CHECK(history.Newer() == std::wstring(L"half typed"));
    CHECK_FALSE(history.Newer().has_value());
    CHECK(history.Older(L"") == std::wstring(L"two"));
}

TEST_CASE("Down without browsing and Up on an empty history do nothing")
{
    ChatHistory history;
    CHECK_FALSE(history.Older(L"draft").has_value());
    CHECK_FALSE(history.Newer().has_value());
    history.Add(L"one");
    CHECK_FALSE(history.Newer().has_value());
}

TEST_CASE("A repeated line moves to the newest place and empty lines are skipped")
{
    ChatHistory history;
    history.Add(L"one");
    history.Add(L"two");
    history.Add(L"one");
    history.Add(L"");

    REQUIRE(history.Lines().size() == 2);
    CHECK(history.Lines()[0] == L"two");
    CHECK(history.Lines()[1] == L"one");
}

TEST_CASE("Sending a line ends browsing")
{
    ChatHistory history;
    history.Add(L"one");
    history.Add(L"two");
    CHECK(history.Older(L"") == std::wstring(L"two"));
    CHECK(history.Older(L"") == std::wstring(L"one"));
    history.Add(L"three");
    CHECK(history.Older(L"") == std::wstring(L"three"));
}

TEST_CASE("The history keeps the newest kCapacity lines")
{
    ChatHistory history;
    for (std::size_t i = 0; i < ChatHistory::kCapacity + 5; ++i)
        history.Add(std::to_wstring(i));

    REQUIRE(history.Lines().size() == ChatHistory::kCapacity);
    CHECK(history.Lines().front() == L"5");
    CHECK(history.Lines().back() == std::to_wstring(ChatHistory::kCapacity + 4));
}
