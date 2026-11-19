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

TEST_CASE("selected monster name line is built only on change [ui][health_bar]")
{
    using UI::Combat::HealthBar::NameLine;
    const std::wstring& first = NameLine(L"Kundun", 0.372f);
    CHECK(first == L"Kundun  37%");
    const std::wstring* address = &first;
    const std::wstring before = first;
    // The same name and health: the same text, from the same place.
    CHECK(&NameLine(L"Kundun", 0.372f) == address);
    CHECK(NameLine(L"Kundun", 0.372f) == before);
    // Health or name changes are seen.
    CHECK(NameLine(L"Kundun", 0.048f) == L"Kundun  4.8%");
    CHECK(NameLine(L"Balgass", 0.048f) == L"Balgass  4.8%");
    // Not known yet or dead: the name alone.
    CHECK(NameLine(L"Balgass", -1.0f) == L"Balgass");
    CHECK(NameLine(L"Balgass", 0.0f) == L"Balgass");
    CHECK(NameLine(L"Kundun", 1.0f) == L"Kundun  100%");
}
