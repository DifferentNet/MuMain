#include "doctest.h"

#include <string>

#include "GameLogic/Items/EnchantFlashLine.h"

using GameLogic::Items::EnchantFlashLine::Parse;

TEST_CASE("The enchant flash line gives the character key and the level")
{
    const auto flash = Parse(L"#UPF|1234|14");
    REQUIRE(flash.has_value());
    CHECK(flash->key == 1234);
    CHECK(flash->level == 14);

    CHECK(Parse(L"#UPF|0|14")->level == 14);
    CHECK(Parse(L"#UPF|32767|15")->key == 32767);
    CHECK(Parse(L"#UPF|7|15\r\n")->level == 15);
}

TEST_CASE("The enchant flash line refuses bad lines")
{
    CHECK_FALSE(Parse(L"").has_value());
    CHECK_FALSE(Parse(L"#UPF|").has_value());
    CHECK_FALSE(Parse(L"#UPF||13").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|").has_value());
    CHECK_FALSE(Parse(L"#UPF|12").has_value());
    CHECK_FALSE(Parse(L"#UPF|abc|13").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|x").has_value());
    CHECK_FALSE(Parse(L"#UPF|-5|13").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|+13").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|13|1").has_value());
    CHECK_FALSE(Parse(L"#UPF| 12|13").has_value());
    CHECK_FALSE(Parse(L"#UPF|32768|13").has_value());
    CHECK_FALSE(Parse(L"#UPF|99999999999|13").has_value());
    CHECK_FALSE(Parse(L"#UPX|12|13").has_value());
    CHECK_FALSE(Parse(L"UPF|12|13").has_value());
}

TEST_CASE("The enchant flash line only takes levels 14 and 15")
{
    CHECK_FALSE(Parse(L"#UPF|12|13").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|12").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|0").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|16").has_value());
    CHECK_FALSE(Parse(L"#UPF|12|7").has_value());
}
