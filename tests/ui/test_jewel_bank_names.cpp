#include <doctest.h>
#include "UI/Inventory/JewelBankNames.h"

#include <string>
#include <vector>

using UI::Inventory::JewelBankNames::SplitInTwo;
using UI::Inventory::JewelBankNames::TwoLines;
using UI::Inventory::JewelBankNames::WithoutSharedLeadingWords;

namespace
{
float Characters(const std::wstring& text)
{
    return static_cast<float>(text.size());
}
} // namespace

TEST_CASE("a long name breaks at the space that evens its two lines")
{
    // Both breaks leave 14 characters on the longer line: the first one wins.
    CHECK(SplitInTwo(L"Lower refining stone", Characters) == TwoLines{L"Lower", L"refining stone"});
    CHECK(SplitInTwo(L"Higher refining stone", Characters) == TwoLines{L"Higher", L"refining stone"});
    CHECK(SplitInTwo(L"ab cd", Characters) == TwoLines{L"ab", L"cd"});
}

TEST_CASE("a name without a space between words stays on one line")
{
    CHECK(SplitInTwo(L"Gemstone", Characters) == TwoLines{L"Gemstone", L""});
    CHECK(SplitInTwo(L" Gemstone ", Characters) == TwoLines{L" Gemstone ", L""});
}

TEST_CASE("jewel names drop the words most of them start with")
{
    const std::vector<std::wstring> names{L"Jewel of Bless", L"Jewel of Soul", L"Jewel of Life",
                                          L"Jewel of Creation", L"Gemstone", L"Lower Refine Stone"};
    const std::vector<std::wstring> expected{L"Bless", L"Soul", L"Life", L"Creation", L"Gemstone",
                                             L"Lower Refine Stone"};
    CHECK(WithoutSharedLeadingWords(names) == expected);
}

TEST_CASE("jewel names with mixed prefixes keep the ones shared by fewer than half")
{
    const std::vector<std::wstring> names{L"Jewel of Bless", L"Jewel of Soul", L"Jewel Box",
                                          L"Stone of Life", L"Stone of Chaos"};
    // "Jewel" starts three of five names, "Jewel of" only two.
    const std::vector<std::wstring> expected{L"of Bless", L"of Soul", L"Box", L"Stone of Life", L"Stone of Chaos"};
    CHECK(WithoutSharedLeadingWords(names) == expected);
}

TEST_CASE("jewel names without shared words stay as they are")
{
    const std::vector<std::wstring> names{L"Jewel of Bless", L"Gemstone", L"Harmony Stone", L"Chaos"};
    CHECK(WithoutSharedLeadingWords(names) == names);
}

TEST_CASE("a single jewel name stays whole")
{
    const std::vector<std::wstring> names{L"Jewel of Bless"};
    CHECK(WithoutSharedLeadingWords(names) == names);
}

TEST_CASE("a name that is only the shared words stays whole")
{
    const std::vector<std::wstring> names{L"Jewel of Bless", L"Jewel of Soul", L"Jewel of"};
    const std::vector<std::wstring> expected{L"Bless", L"Soul", L"Jewel of"};
    CHECK(WithoutSharedLeadingWords(names) == expected);
}

TEST_CASE("Russian jewel names split on spaces only")
{
    const std::vector<std::wstring> names{L"Камень души",
                                          L"Камень жизни",
                                          L"Камень хаоса",
                                          L"Каменщик"};
    // "Камень" is dropped; "Каменщик" shares its first letters but not the word, so it stays.
    const std::vector<std::wstring> expected{L"души", L"жизни",
                                             L"хаоса",
                                             L"Каменщик"};
    CHECK(WithoutSharedLeadingWords(names) == expected);
}
