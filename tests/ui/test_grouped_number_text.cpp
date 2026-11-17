#include <doctest.h>

#include "UI/Text/GroupedNumberText.h"

using UI::Text::FormatGroupedNumbers;
using UI::Text::GroupThousands;

TEST_CASE("numbers are grouped by thousands [ui][text]")
{
    CHECK(GroupThousands(0) == L"0");
    CHECK(GroupThousands(999) == L"999");
    CHECK(GroupThousands(1'000) == L"1,000");
    CHECK(GroupThousands(1'234'567) == L"1,234,567");
    CHECK(GroupThousands(4'294'967'295LL) == L"4,294,967,295");
    CHECK(GroupThousands(-1'500) == L"-1,500");
}

TEST_CASE("each integer placeholder takes the next value in order [ui][text]")
{
    CHECK(FormatGroupedNumbers(L"Obtained %d Exp", { 1'234'567 }) == L"Obtained 1,234,567 Exp");
    CHECK(FormatGroupedNumbers(L"Combine %d (%d zen is required)", { 10, 500'000 })
          == L"Combine 10 (500,000 zen is required)");
    CHECK(FormatGroupedNumbers(L"Zen: %lu", { 2'000'000 }) == L"Zen: 2,000,000");
    CHECK(FormatGroupedNumbers(L"%I64d and %lld", { 1'000, 2'000 }) == L"1,000 and 2,000");
}

TEST_CASE("other placeholders and missing values stay as written [ui][text]")
{
    CHECK(FormatGroupedNumbers(L"100%% of %d", { 5'000 }) == L"100% of 5,000");
    CHECK(FormatGroupedNumbers(L"%ls got %d", { 7 }) == L"%ls got 7");
    CHECK(FormatGroupedNumbers(L"%d and %d", { 1'000 }) == L"1,000 and %d");
    CHECK(FormatGroupedNumbers(nullptr, { 1 }).empty());
}
