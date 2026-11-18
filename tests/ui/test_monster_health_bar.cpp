#include <doctest.h>

#include "UI/Combat/MonsterHealthBar.h"

TEST_CASE("selected monster health eligibility [ui][health_bar]")
{
    CHECK(UI::Combat::HealthBar::ShouldRenderSelected(1.0f));
    CHECK(UI::Combat::HealthBar::ShouldRenderSelected(0.25f));
    CHECK(UI::Combat::HealthBar::ShouldRenderSelected(-1.0f));
    CHECK_FALSE(UI::Combat::HealthBar::ShouldRenderSelected(0.0f));
}

TEST_CASE("selected monster health percent text [ui][health_bar]")
{
    using UI::Combat::HealthBar::PercentText;
    CHECK(PercentText(-1.0f).empty());
    CHECK(PercentText(1.0f) == L"100%");
    CHECK(PercentText(0.372f) == L"37%");
    CHECK(PercentText(0.048f) == L"4.8%");
    CHECK(PercentText(0.0f).empty());
    CHECK(PercentText(0.004f) == L"0.4%");
    CHECK(PercentText(0.1f) == L"10%");
}
