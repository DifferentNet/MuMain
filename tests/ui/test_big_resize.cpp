#include <doctest.h>

#include "UI/Mobile/BigResize.h"

using UI::Mobile::IsBigResize;

TEST_CASE("The address bar sliding keeps the held fingers")
{
    // A phone held sideways: the bar takes about 56 CSS pixels of 412.
    CHECK_FALSE(IsBigResize(915, 356, 915, 412));
    CHECK_FALSE(IsBigResize(915, 412, 915, 356));
    CHECK_FALSE(IsBigResize(1280, 720, 1280, 720));
}

TEST_CASE("A turned phone or a much bigger change drops them")
{
    CHECK(IsBigResize(915, 412, 412, 915));
    CHECK(IsBigResize(915, 412, 915, 300));
    CHECK(IsBigResize(740, 360, 1024, 360));
}

TEST_CASE("A missing size counts as big")
{
    CHECK(IsBigResize(0, 0, 915, 412));
    CHECK(IsBigResize(915, 412, 0, 412));
}
