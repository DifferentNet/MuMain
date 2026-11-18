#include <doctest.h>

#include "GameLogic/Items/CraftingRefusal.h"
#include "UI/Chat/BagRoomWarning.h"
#include "UI/Chat/BuffEndNotices.h"
#include "UI/HUD/LowStockWarning.h"

#include <vector>

namespace Refusal = GameLogic::Items::CraftingRefusal;
using Refusal::Reason;
using UI::Chat::BagRoomWarning;
using UI::Chat::BuffEndNotices;
using UI::Hud::LowStockWarning;

TEST_CASE("crafting refusal codes map to their reason [ui][round12]")
{
    constexpr int anyMix = 40;
    CHECK(Refusal::ReasonOf(3, anyMix) == Reason::TooManyItems);
    CHECK(Refusal::ReasonOf(6, anyMix) == Reason::ItemsMissing);
    CHECK(Refusal::ReasonOf(7, anyMix) == Reason::ImproperItems);
    CHECK(Refusal::ReasonOf(10, anyMix) == Reason::ImproperItems);
    CHECK(Refusal::ReasonOf(8, anyMix) == Reason::ItemLevel);
    CHECK(Refusal::ReasonOf(4, anyMix) == Reason::LevelTooLow);
    CHECK(Refusal::ReasonOf(9, anyMix) == Reason::ClassCannot);
    CHECK(Refusal::ReasonOf(4, Refusal::DevilSquareTicketMixId) == Reason::DevilSquareLevel);
    CHECK(Refusal::ReasonOf(9, Refusal::BloodCastleTicketMixId) == Reason::BloodCastleLevel);
    // The ticket boxes stay with their own recipe only.
    CHECK(Refusal::ReasonOf(4, Refusal::BloodCastleTicketMixId) == Reason::LevelTooLow);
    CHECK(Refusal::ReasonOf(9, Refusal::DevilSquareTicketMixId) == Reason::ClassCannot);
    // Success, failure and unknown codes are not refusals.
    CHECK(Refusal::ReasonOf(0, anyMix) == Reason::None);
    CHECK(Refusal::ReasonOf(1, anyMix) == Reason::None);
    CHECK(Refusal::ReasonOf(2, anyMix) == Reason::None);
    CHECK(Refusal::ReasonOf(5, anyMix) == Reason::None);
    CHECK(Refusal::ReasonOf(99, anyMix) == Reason::None);
}

TEST_CASE("buff end is said after the delay and joined with others [ui][round12]")
{
    BuffEndNotices notices;
    notices.Ended(7, 1000);
    CHECK(notices.TakeDue(1000).empty());
    notices.Ended(9, 1200);
    CHECK(notices.TakeDue(1600).empty());
    CHECK(notices.TakeDue(1700) == std::vector<int>{7, 9});
    CHECK(notices.TakeDue(5000).empty());
}

TEST_CASE("buff that comes back before the delay is not said [ui][round12]")
{
    BuffEndNotices notices;
    notices.Ended(7, 1000);
    notices.Started(7);
    CHECK(notices.TakeDue(2000).empty());
}

TEST_CASE("buff end duplicates are said once [ui][round12]")
{
    BuffEndNotices notices;
    notices.Ended(7, 1000);
    notices.Ended(7, 1100);
    CHECK(notices.TakeDue(1600) == std::vector<int>{7});
}

TEST_CASE("buff ends that keep coming are said after the longest wait [ui][round12]")
{
    BuffEndNotices notices;
    for (std::uint32_t t = 0; t < BuffEndNotices::LongestWaitMs; t += 400)
    {
        notices.Ended(static_cast<int>(t), 1000 + t);
        CHECK(notices.TakeDue(1000 + t).empty());
    }
    CHECK_FALSE(notices.TakeDue(1000 + BuffEndNotices::LongestWaitMs).empty());
}

TEST_CASE("buff ends after a map change or death are not said [ui][round12]")
{
    BuffEndNotices notices;
    notices.Ended(7, 1000);
    notices.Silence(1100);
    CHECK(notices.TakeDue(2000).empty());
    notices.Ended(8, 1100 + BuffEndNotices::QuietAfterMapMs - 1);
    CHECK(notices.TakeDue(10000).empty());
    notices.Ended(8, 1100 + BuffEndNotices::QuietAfterMapMs);
    CHECK(notices.TakeDue(10000) == std::vector<int>{8});

    notices.Ended(9, 20000);
    notices.Clear();
    CHECK(notices.TakeDue(30000).empty());
}

TEST_CASE("buff end delay works across the clock wrap [ui][round12]")
{
    BuffEndNotices notices;
    notices.Ended(7, 0xFFFFFF00u);
    CHECK(notices.TakeDue(0xFFFFFF10u).empty());
    CHECK(notices.TakeDue(0x00000100u) == std::vector<int>{7});
}

TEST_CASE("bag room warning is said once per crossing [ui][round12]")
{
    BagRoomWarning bag;
    CHECK(bag.Note(20) == BagRoomWarning::Say::Nothing);
    CHECK(bag.Note(9) == BagRoomWarning::Say::Nothing);
    CHECK(bag.Note(8) == BagRoomWarning::Say::AlmostFull);
    CHECK(bag.Note(5) == BagRoomWarning::Say::Nothing);
    CHECK(bag.Note(0) == BagRoomWarning::Say::Full);
    CHECK(bag.Note(0) == BagRoomWarning::Say::Nothing);
    // Freed a little, still almost full: quiet; full again: quiet until it went above the mark.
    CHECK(bag.Note(3) == BagRoomWarning::Say::Nothing);
    CHECK(bag.Note(0) == BagRoomWarning::Say::Nothing);
    CHECK(bag.Note(9) == BagRoomWarning::Say::Nothing);
    CHECK(bag.Note(7) == BagRoomWarning::Say::AlmostFull);
}

TEST_CASE("bag that fills at once says full [ui][round12]")
{
    BagRoomWarning bag;
    CHECK(bag.Note(12) == BagRoomWarning::Say::Nothing);
    CHECK(bag.Note(0) == BagRoomWarning::Say::Full);
    CHECK(bag.Note(2) == BagRoomWarning::Say::Nothing);
}

TEST_CASE("low potion line falls once from above the mark [ui][round12]")
{
    constexpr int potion = 100;
    LowStockWarning low;
    CHECK_FALSE(low.Note(potion, 0, 12, 0));
    CHECK_FALSE(low.Note(potion, 0, 11, 100));
    CHECK(low.Note(potion, 0, 10, 200));
    CHECK_FALSE(low.Note(potion, 0, 9, 300));
    CHECK_FALSE(low.Note(potion, 0, 0, 400));
    // Bought more but within a minute of the last line: quiet; later it is said again.
    CHECK_FALSE(low.Note(potion, 0, 30, 500));
    CHECK_FALSE(low.Note(potion, 0, 10, 600));
    CHECK_FALSE(low.Note(potion, 0, 30, 70000));
    CHECK(low.Note(potion, 0, 8, 70100));
}

TEST_CASE("low potion count followed by hand is never said [ui][round12]")
{
    LowStockWarning low;
    CHECK_FALSE(low.Note(100, 0, 30, 0));
    low.Follow(100, 0, 5);                // the stack put into the vault
    CHECK_FALSE(low.Note(100, 0, 4, 100));
    low.Follow(100, 0, 30);               // taken back
    CHECK(low.Note(100, 0, 10, 200));
}

TEST_CASE("low potion line starts over for another item [ui][round12]")
{
    LowStockWarning low;
    CHECK_FALSE(low.Note(100, 0, 5, 0));   // first sight below the mark: nothing
    CHECK_FALSE(low.Note(100, 0, 4, 100));
    CHECK_FALSE(low.Note(101, 0, 20, 200)); // another potion put on the slot
    CHECK(low.Note(101, 0, 10, 300));
    CHECK_FALSE(low.Note(101, 1, 3, 400));  // another level of it is another item
}
