#include <doctest.h>

#include "UI/Chat/ChatRowWrap.h"

using UI::Chat::ChatRowSource;
using UI::Chat::ChatRowTooNarrow;
using UI::Chat::ChooseChatRowSource;
using UI::Chat::ChatRowWidth;
using UI::Chat::WrapChatRows;

namespace
{
using Rows = std::vector<std::wstring>;

// One unit per character, a 'W' two: the rows below are counted by hand.
float Units(const std::wstring& row)
{
    float width = 0.f;
    for (const wchar_t c : row)
        width += c == L'W' ? 2.f : 1.f;
    return width;
}
} // namespace

TEST_CASE("A message that fits stays one row")
{
    CHECK(WrapChatRows(L"Bob : ", L"hello there", 20.f, Units) == Rows{L"hello there"});
}

TEST_CASE("A row exactly as wide as the log fits")
{
    // "Bob : " (6) + "hello there" (11) = 17.
    CHECK(WrapChatRows(L"Bob : ", L"hello there", 17.f, Units) == Rows{L"hello there"});
    CHECK(WrapChatRows(L"Bob : ", L"hello there", 16.f, Units) == Rows{L"hello", L"there"});
}

TEST_CASE("Only the first row is measured with the sender in front")
{
    // With the prefix "aaa bbb" would be 13 wide; the second row has no prefix and takes it.
    CHECK(WrapChatRows(L"Bob : ", L"aaa bbb ccc", 10.f, Units) == Rows{L"aaa", L"bbb ccc"});
}

TEST_CASE("A long message takes as many rows as it needs")
{
    CHECK(WrapChatRows(L"", L"one two three four five six", 9.f, Units)
          == Rows{L"one two", L"three", L"four five", L"six"});
}

TEST_CASE("The measure decides, not the number of characters")
{
    // "WWW" is 6 wide, as wide as "aaaaaa".
    CHECK(WrapChatRows(L"", L"WWW aa", 8.f, Units) == Rows{L"WWW", L"aa"});
    CHECK(WrapChatRows(L"", L"aaa aa", 8.f, Units) == Rows{L"aaa aa"});
}

TEST_CASE("A word wider than a row breaks between characters")
{
    CHECK(WrapChatRows(L"", L"abcdefghijkl", 5.f, Units) == Rows{L"abcde", L"fghij", L"kl"});
    // The words before it keep their row; the long word starts the next one.
    CHECK(WrapChatRows(L"", L"hi abcdefghijkl", 5.f, Units) == Rows{L"hi", L"abcde", L"fghij", L"kl"});
}

TEST_CASE("The sender counts as the first row's first word")
{
    // "abcdefgh" fits a row of its own but not after "Al : ": it goes whole to the second row, and the first
    // holds the sender alone.
    CHECK(WrapChatRows(L"Al : ", L"abcdefgh", 8.f, Units) == Rows{L"", L"abcdefgh"});
    // A word wider than a row breaks only on a row of its own.
    CHECK(WrapChatRows(L"Al : ", L"abcdefghijkl", 8.f, Units) == Rows{L"", L"abcdefgh", L"ijkl"});
    // Spaces in front of that word go with the break.
    CHECK(WrapChatRows(L"Al : ", L"  abcdefgh", 8.f, Units) == Rows{L"", L"abcdefgh"});
    // A word that fits after the sender stays with it.
    CHECK(WrapChatRows(L"Al : ", L"abc defgh", 8.f, Units) == Rows{L"abc", L"defgh"});
}

TEST_CASE("A sender wider than a row still leaves every character on a row")
{
    // "Alexander : " is 12 wide: the first row takes one character after it, the rest go on as usual.
    CHECK(WrapChatRows(L"Alexander : ", L"abc def", 5.f, Units) == Rows{L"a", L"bc", L"def"});
    CHECK(WrapChatRows(L"Alexander : ", L"x", 5.f, Units) == Rows{L"x"});
}

TEST_CASE("The spaces at a break go")
{
    CHECK(WrapChatRows(L"", L"aaa   bbb", 4.f, Units) == Rows{L"aaa", L"bbb"});
}

TEST_CASE("A row too narrow for any character still ends")
{
    CHECK(WrapChatRows(L"", L"abc", 0.f, Units) == Rows{L"a", L"b", L"c"});
}

TEST_CASE("An empty message is one empty row")
{
    CHECK(WrapChatRows(L"Bob : ", L"", 10.f, Units) == Rows{L""});
}

TEST_CASE("A row keeps a margin of its width free, at least two px")
{
    CHECK(ChatRowWidth(400.f) == doctest::Approx(390.f));
    CHECK(ChatRowWidth(40.f) == doctest::Approx(38.f));
}

TEST_CASE("A row narrower than a few wide letters is too narrow to split for")
{
    // MinRowSample is five 'W', ten units here.
    CHECK(ChatRowTooNarrow(9.9f, Units));
    CHECK_FALSE(ChatRowTooNarrow(10.f, Units));
    CHECK(ChatRowTooNarrow(0.f, Units));
}

TEST_CASE("A message waits for the log's hidden row only while that row can still come")
{
    // No RmlUi: the native font at once.
    CHECK(ChooseChatRowSource(false, false, false, 0.f) == ChatRowSource::NativeFont);
    // The log's document not built, or not updated by RmlUi yet: wait.
    CHECK(ChooseChatRowSource(true, false, false, 0.f) == ChatRowSource::Wait);
    CHECK(ChooseChatRowSource(true, false, true, 0.f) == ChatRowSource::Wait);
    // Updated but not laid out yet: wait; laid out: the log's own font.
    CHECK(ChooseChatRowSource(true, true, true, 0.f) == ChatRowSource::Wait);
    CHECK(ChooseChatRowSource(true, true, true, 281.f) == ChatRowSource::LogFont);
    // Updated, and its markup has no hidden row (an older or overridden chat_log.rml): the native font, never a
    // wait that would last for good.
    CHECK(ChooseChatRowSource(true, true, false, 0.f) == ChatRowSource::NativeFont);
}
