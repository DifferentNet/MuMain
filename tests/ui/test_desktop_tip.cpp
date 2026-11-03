#include <doctest.h>

#include "UI/Mobile/DesktopTip.h"

using UI::Mobile::DesktopTip::NeedsMouseOrKeys;

TEST_CASE("tips that name the mouse or a key are found [ui][desktop-tip]")
{
    CHECK(NeedsMouseOrKeys(L"To use skills or magic, right click on monster."));
    CHECK(NeedsMouseOrKeys(L"Press Q, W, E to use the items registered under those keys."));
    CHECK(NeedsMouseOrKeys(L"Point the cursor on the skill button and press ctrl+number key to set hotkey."));
    CHECK(NeedsMouseOrKeys(L"To use the saved chatting macro, press Alt+number."));
    CHECK(NeedsMouseOrKeys(L"Press space bar to collect loot."));
    CHECK(NeedsMouseOrKeys(L"Click shortcut key T to check the quest information."));
    CHECK(NeedsMouseOrKeys(L"The supplementary magic of the Elf can be used by clicking the name in the party window."));
}

TEST_CASE("tips that fit any controls stay [ui][desktop-tip]")
{
    CHECK_FALSE(NeedsMouseOrKeys(L"You must be at least level 15 to enter Devias"));
    CHECK_FALSE(NeedsMouseOrKeys(L"5 stat points are rewarded each time you level up."));
    CHECK_FALSE(NeedsMouseOrKeys(L"To send a message to guild members, type '@ message'"));
    CHECK_FALSE(NeedsMouseOrKeys(L"Excellent type item names are displayed in green."));
    CHECK_FALSE(NeedsMouseOrKeys(L""));
}
