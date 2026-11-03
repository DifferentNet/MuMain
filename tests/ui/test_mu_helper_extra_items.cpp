// The MU Helper's extra item names survive a save and load (MUHelper::ConfigDataSerDe).
// Names of 15 bytes or more were dropped on save, so "Symbol of Kundun" was gone after reopening the window
// (ghinzuro, 2026-11-03).

#include "App/stdafx.h"

#include <doctest.h>

#include "MUHelper/MuHelperData.h"

#include <cstdlib>
#include <cstring>
#include <string>

using MUHelper::ConfigData;
using MUHelper::ConfigDataSerDe;
using MUHelper::FitExtraItemName;

namespace
{
ConfigData RoundTrip(const ConfigData& in)
{
    PRECEIVE_MUHELPER_DATA packet{};
    ConfigDataSerDe::Serialize(in, packet);
    ConfigData out;
    ConfigDataSerDe::Deserialize(packet, out);
    return out;
}
} // namespace

TEST_CASE("a long extra item name is cut to 15 bytes, not dropped [ui][muhelper]")
{
    ConfigData c;
    c.bPickExtraItems = true;
    c.aExtraItems = { L"Symbol of Kundun", L"Jewel" };

    const ConfigData out = RoundTrip(c);
    CHECK(out.bPickExtraItems);
    CHECK(out.aExtraItems.size() == 2);
    CHECK(out.aExtraItems.contains(L"Symbol of Kundu"));
    CHECK(out.aExtraItems.contains(L"Jewel"));
}

TEST_CASE("a name of exactly 15 bytes survives whole [ui][muhelper]")
{
    ConfigData c;
    c.aExtraItems = { L"Box of Kundun+5" };
    CHECK(RoundTrip(c).aExtraItems.contains(L"Box of Kundun+5"));
}

TEST_CASE("Cyrillic names survive and are cut on a character boundary [ui][muhelper]")
{
    ConfigData c;
    c.aExtraItems = { L"\u041A\u0430\u043C\u0435\u043D\u044C", L"\u0421\u0438\u043C\u0432\u043E\u043B \u041A\u0443\u043D\u0434\u0443\u043D\u0430" };
    const ConfigData out = RoundTrip(c);
    CHECK(out.aExtraItems.contains(L"\u041A\u0430\u043C\u0435\u043D\u044C"));
    // "Камень" and "Символ Кундуна": a letter is 2 bytes, so "Символ К" is 15 bytes and the next letter would not fit.
    CHECK(out.aExtraItems.contains(L"\u0421\u0438\u043C\u0432\u043E\u043B \u041A"));
}

TEST_CASE("twelve names are kept [ui][muhelper]")
{
    ConfigData c;
    for (int i = 0; i < 12; ++i)
        c.aExtraItems.insert(L"Item " + std::to_wstring(i));
    CHECK(RoundTrip(c).aExtraItems.size() == 12);
}

TEST_CASE("the fitted name is trimmed [ui][muhelper]")
{
    CHECK(FitExtraItemName(L"  Jewel  ") == L"Jewel");
    CHECK(FitExtraItemName(L"   ").empty());
    CHECK(FitExtraItemName(L"Symbol of Kundun") == L"Symbol of Kundu");
    // A cut that ends on a space drops the space.
    CHECK(FitExtraItemName(L"Jewel of Chaos 1") == L"Jewel of Chaos");
}

TEST_CASE("the quest items switch survives a save and load and leaves the boxes switch alone [ui][muhelper]")
{
    ConfigData c;
    c.bPickQuestItems = true;
    ConfigData out = RoundTrip(c);
    CHECK(out.bPickQuestItems);
    CHECK_FALSE(out.bPickContainers);

    c.bPickQuestItems = false;
    c.bPickContainers = true;
    out = RoundTrip(c);
    CHECK_FALSE(out.bPickQuestItems);
    CHECK(out.bPickContainers);
}

TEST_CASE("a name an older client saved in the PC code page loads as the older client read it [ui][muhelper]")
{
    // "\u041A\u0430\u043C\u0435\u043D\u044C" in CP1251, as wcstombs wrote it on a Russian Windows before 2026-11-03.
    // It is not UTF-8, so it must not turn into U+FFFD: it is read with mbstowcs, as the older client did.
    const char cp1251[] = "\xCA\xE0\xEC\xE5\xED\xFC";
    wchar_t expected[16] = {};
    REQUIRE(std::mbstowcs(expected, cp1251, 15) == 6);

    PRECEIVE_MUHELPER_DATA packet{};
    std::memcpy(packet.ExtraItems[0], cp1251, sizeof(cp1251) - 1);
    ConfigData out;
    ConfigDataSerDe::Deserialize(packet, out);

    REQUIRE(out.aExtraItems.size() == 1);
    CHECK(*out.aExtraItems.begin() == std::wstring(expected));
    CHECK(out.aExtraItems.begin()->find(L'\uFFFD') == std::wstring::npos);
    // Saved again it is UTF-8 and loads the same.
    CHECK(RoundTrip(out).aExtraItems.contains(std::wstring(expected)));
}
