#pragma once

// MU Moldova: the account's Excellent Dust for the Chaos Goblin (server/addons/src/ExcellentDust.cs). The goblin's
// method menu has an "Excellent Dust" button: it opens the regular machine (recipes 53 and 54 are regular ones) in
// a dust mode that shows the counter. The counter comes from the hidden /dustinfo answer
// "#EXD|dust|earned this week|weekly limit|seconds to the reset|dust an item takes[|dust added at the reset]"
// ("#EXD!|" without the dust);
// that line stays out of the chat.

#include <string>

namespace GameLogic::ExcellentDust
{
    // Asks the server for the counter (/dustinfo). Repeats within a second are dropped, unless forced (after a mix,
    // when the dust changed).
    void Request(bool force = false);

    // The machine was opened from the menu's dust button; cleared when the machine opens or closes.
    bool IsDustMode();
    void SetDustMode(bool on);

    // "Dust: 1,234 (an item takes 250)", or a wait / "not available" line before the answer.
    std::wstring CounterLine();
    // "This week 120 of 1,000, resets in 2d 5h"; empty until the answer.
    std::wstring WeekLine();
}
