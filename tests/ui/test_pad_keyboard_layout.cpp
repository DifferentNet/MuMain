#include <doctest.h>

#include "UI/HUD/PadKeyboardLayout.h"

#include <cmath>
#include <string_view>

namespace Layout = UI::HUD::PadKeyboardLayout;
using Layout::Action;
using Layout::Page;
using Layout::Spot;

namespace
{
constexpr Page AllPages[] = {Page::Latin, Page::Cyrillic, Page::Symbols, Page::Digits};

std::string_view TypedAt(Page page, Spot spot)
{
    return Layout::KeyAt(page, spot).lower;
}

float RowWidth(Page page, int row)
{
    float width = 0.f;
    for (const Layout::Key& key : Layout::Rows(page)[row])
        width += key.width;
    return width;
}
} // namespace

TEST_CASE("every page has a way out and the keys a field needs [ui][pad-keyboard]")
{
    for (const Page page : AllPages)
    {
        CHECK(Layout::KeyAt(page, Layout::Find(page, Action::Backspace)).action == Action::Backspace);
        CHECK(Layout::KeyAt(page, Layout::Find(page, Action::Enter)).action == Action::Enter);
    }
    for (const Page page : {Page::Latin, Page::Cyrillic})
    {
        CHECK(Layout::KeyAt(page, Layout::Find(page, Action::Shift)).action == Action::Shift);
        CHECK(Layout::KeyAt(page, Layout::Find(page, Action::Symbols)).action == Action::Symbols);
        CHECK(Layout::KeyAt(page, Layout::Find(page, Action::Language)).action == Action::Language);
        CHECK(Layout::KeyAt(page, Layout::Find(page, Action::Space)).action == Action::Space);
    }
    CHECK(Layout::KeyAt(Page::Symbols, Layout::Find(Page::Symbols, Action::Letters)).action == Action::Letters);
}

TEST_CASE("the Russian page has all 33 letters, each with its capital [ui][pad-keyboard]")
{
    constexpr std::string_view Alphabet[] = {"а", "б", "в", "г", "д", "е", "ё", "ж", "з", "и", "й",
                                             "к", "л", "м", "н", "о", "п", "р", "с", "т", "у", "ф",
                                             "х", "ц", "ч", "ш", "щ", "ъ", "ы", "ь", "э", "ю", "я"};
    for (const std::string_view letter : Alphabet)
    {
        bool found = false;
        for (const auto row : Layout::Rows(Page::Cyrillic))
        {
            for (const Layout::Key& key : row)
            {
                if (key.lower == letter)
                {
                    found = true;
                    CHECK(key.upper != key.lower);
                }
            }
        }
        CHECK_MESSAGE(found, letter);
    }
}

TEST_CASE("no row is wider than its page [ui][pad-keyboard]")
{
    for (const Page page : AllPages)
    {
        const int rows = static_cast<int>(Layout::Rows(page).size());
        CHECK(rows >= 4);
        for (int row = 0; row < rows; ++row)
            CHECK(RowWidth(page, row) <= Layout::Width(page) + 0.001f);
    }
}

TEST_CASE("left and right go along the row and round its ends [ui][pad-keyboard]")
{
    CHECK(TypedAt(Page::Latin, Layout::Step(Page::Latin, {1, 0}, 1, 0)) == "w");
    CHECK(TypedAt(Page::Latin, Layout::Step(Page::Latin, {1, 0}, -1, 0)) == "p");
    CHECK(TypedAt(Page::Latin, Layout::Step(Page::Latin, {1, 9}, 1, 0)) == "q");
}

TEST_CASE("up and down go to the nearest key of the next row [ui][pad-keyboard]")
{
    // The home row sits half a key in: q goes down to a, p to l.
    CHECK(TypedAt(Page::Latin, Layout::Step(Page::Latin, {1, 0}, 0, 1)) == "a");
    CHECK(TypedAt(Page::Latin, Layout::Step(Page::Latin, {1, 9}, 0, 1)) == "l");
    // From the space bar up to the letters over its middle, and back down to it.
    const Spot space = Layout::Find(Page::Latin, Action::Space);
    const Spot above = Layout::Step(Page::Latin, space, 0, -1);
    CHECK(above.row == space.row - 1);
    CHECK(Layout::Step(Page::Latin, above, 0, 1) == space);
    // Round from the top row to the bottom one and back.
    const Spot bottom = Layout::Step(Page::Latin, {0, 0}, 0, -1);
    CHECK(bottom.row == static_cast<int>(Layout::Rows(Page::Latin).size()) - 1);
    CHECK(Layout::Step(Page::Latin, bottom, 0, 1).row == 0);
}

TEST_CASE("a spot from another page is kept on this one [ui][pad-keyboard]")
{
    const Spot far{4, 5};
    const Spot kept = Layout::Clamp(Page::Digits, far);
    CHECK(kept.row == 3);
    CHECK(kept.column == 2);
    CHECK(Layout::Clamp(Page::Digits, {-1, -1}) == Spot{0, 0});
}
