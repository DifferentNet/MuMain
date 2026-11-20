#include <doctest.h>

#include "UI/Chat/ChatRowWrap.h"

#include <utility>
#include <vector>

using UI::Chat::ChatRowSource;
using UI::Chat::ChatRowTooNarrow;
using UI::Chat::ChooseChatRowSource;
using UI::Chat::ChatRowWidth;
using UI::Chat::WrapChatRows;
using UI::Chat::WrapChatRowSpans;
using UI::Chat::ChatTextSpan;

namespace
{
using Rows = std::vector<std::wstring>;
using Pieces = std::vector<std::pair<std::size_t, std::size_t>>;

Pieces AsPairs(const std::vector<ChatTextSpan>& spans)
{
    Pieces pairs;
    for (const ChatTextSpan& span : spans)
        pairs.emplace_back(span.start, span.length);
    return pairs;
}

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

TEST_CASE("Rows as pieces of the text, the spaces at a break left out")
{
    CHECK(AsPairs(WrapChatRowSpans(L"", L"one two three four five six", 9.f, Units))
          == Pieces{{0, 7}, {8, 5}, {14, 9}, {24, 3}});
    CHECK(AsPairs(WrapChatRowSpans(L"", L"", 9.f, Units)) == Pieces{{0, 0}});
    // The sender's row alone holds no text and the next starts at the same place.
    CHECK(AsPairs(WrapChatRowSpans(L"Al : ", L"abcdefgh", 8.f, Units)) == Pieces{{0, 0}, {0, 8}});
}

TEST_CASE("An item's name stays whole in one row")
{
    // "[Bone Blade +15]" is 16 wide: without the piece the row breaks inside the name.
    CHECK(WrapChatRows(L"", L"see [Bone Blade +15] now", 16.f, Units) == Rows{L"see [Bone Blade", L"+15] now"});
    CHECK(AsPairs(WrapChatRowSpans(L"", L"see [Bone Blade +15] now", 16.f, Units, {{4, 16}}))
          == Pieces{{0, 3}, {4, 16}, {21, 3}});
    // After the sender: the name that does not fit after it goes whole to the second row.
    CHECK(AsPairs(WrapChatRowSpans(L"Al : ", L"[Ab Cd] ef", 8.f, Units, {{0, 7}})) == Pieces{{0, 0}, {0, 7}, {8, 2}});
}

TEST_CASE("A word wider than a row breaks before an item's name, and only a name wider than a row is cut")
{
    CHECK(AsPairs(WrapChatRowSpans(L"", L"xx[Ab Cd]", 7.f, Units, {{2, 7}})) == Pieces{{0, 2}, {2, 7}});
    CHECK(AsPairs(WrapChatRowSpans(L"", L"xx[Ab Cd]", 6.f, Units, {{2, 7}})) == Pieces{{0, 2}, {2, 6}, {8, 1}});
}
