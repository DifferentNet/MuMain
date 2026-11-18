#include <doctest.h>

#include "GameLogic/Combat/DuelReplies.h"
#include "UI/Chat/DeclinedInviteLines.h"
#include "UI/Chat/HelperRunSummary.h"

#include <cstdint>
#include <string>

using GameLogic::Combat::ClassifyDuelStart;
using GameLogic::Combat::DuelEndNotice;
using GameLogic::Combat::DuelStartReply;
using UI::Chat::DeclinedInviteLines;
using UI::Chat::HelperRunSummary;
using UI::Chat::InviteKind;

TEST_CASE("duel start answers map to what the player is told [ui][round14]")
{
    CHECK(ClassifyDuelStart(0) == DuelStartReply::Accepted);
    CHECK(ClassifyDuelStart(12) == DuelStartReply::TooLowLevel);
    CHECK(ClassifyDuelStart(28) == DuelStartReply::TooLowLevel);
    CHECK(ClassifyDuelStart(14) == DuelStartReply::Failed);
    CHECK(ClassifyDuelStart(15) == DuelStartReply::Declined);
    CHECK(ClassifyDuelStart(16) == DuelStartReply::ArenaFull);
    CHECK(ClassifyDuelStart(30) == DuelStartReply::NotEnoughZen);
    CHECK(ClassifyDuelStart(1) == DuelStartReply::Unknown);
    CHECK(ClassifyDuelStart(-1) == DuelStartReply::Unknown);
}

TEST_CASE("a finished duel never says it was canceled [ui][round14]")
{
    DuelEndNotice notice;
    notice.Started();
    notice.Finished();
    CHECK_FALSE(notice.OnEnded(true, 10'000)); // to both duellists
    CHECK_FALSE(notice.OnEnded(true, 10'005)); // at once to everybody in the arena
}

TEST_CASE("a duel left half way says canceled once [ui][round14]")
{
    DuelEndNotice notice;
    notice.Started();
    CHECK(notice.OnEnded(true, 10'000));
    CHECK_FALSE(notice.OnEnded(true, 10'005));
}

TEST_CASE("a duel broken in the arena before it began says canceled once [ui][round14]")
{
    DuelEndNotice notice; // the countdown: no start answer yet
    CHECK(notice.OnEnded(true, 5'000));
    CHECK_FALSE(notice.OnEnded(true, 5'010));
    // The next duel broken the same way, later, is said again.
    CHECK(notice.OnEnded(true, 5'000 + DuelEndNotice::BurstMs + 60'000));
}

TEST_CASE("a duel that never started says nothing on duel ended [ui][round14]")
{
    DuelEndNotice notice;
    CHECK_FALSE(notice.OnEnded(false, 1'000)); // the "duel ended" sent before a failed start (14)
    notice.Started();
    notice.Reset();                            // character switch mid-duel
    CHECK_FALSE(notice.OnEnded(false, 90'000));
}

TEST_CASE("the duel ended burst survives the tick counter wrap [ui][round14]")
{
    DuelEndNotice notice;
    notice.Started();
    CHECK(notice.OnEnded(true, 0xFFFF'FFFFu - 100));
    CHECK_FALSE(notice.OnEnded(true, 200));
}

TEST_CASE("a refused invitation is said once per sender and kind in the quiet time [ui][round14]")
{
    DeclinedInviteLines lines;
    CHECK(lines.ShouldSay(InviteKind::Party, L"Spammer", 1'000));
    CHECK_FALSE(lines.ShouldSay(InviteKind::Party, L"Spammer", 1'000 + DeclinedInviteLines::QuietMs - 1));
    CHECK(lines.ShouldSay(InviteKind::Trade, L"Spammer", 2'000));
    CHECK(lines.ShouldSay(InviteKind::Party, L"Other", 2'000));
    CHECK(lines.ShouldSay(InviteKind::Party, L"Spammer", 1'000 + DeclinedInviteLines::QuietMs));
}

TEST_CASE("the invitation quiet time survives the tick counter wrap [ui][round14]")
{
    DeclinedInviteLines lines;
    const std::uint32_t nearWrap = 0xFFFF'FFFFu - 1'000;
    CHECK(lines.ShouldSay(InviteKind::Duel, L"A", nearWrap));
    CHECK_FALSE(lines.ShouldSay(InviteKind::Duel, L"A", 2'000));
    CHECK(lines.ShouldSay(InviteKind::Duel, L"A", nearWrap + DeclinedInviteLines::QuietMs));
}

TEST_CASE("the oldest remembered sender is forgotten first [ui][round14]")
{
    DeclinedInviteLines lines;
    for (std::size_t i = 0; i < DeclinedInviteLines::MaxRemembered; ++i)
        CHECK(lines.ShouldSay(InviteKind::Party, L"P" + std::to_wstring(i), static_cast<std::uint32_t>(100 + i)));
    // One more sender pushes out P0, the oldest; P1 is still remembered.
    CHECK(lines.ShouldSay(InviteKind::Party, L"New", 200));
    CHECK(lines.ShouldSay(InviteKind::Party, L"P0", 300));
    CHECK_FALSE(lines.ShouldSay(InviteKind::Party, L"P2", 300));
    lines.Clear();
    CHECK(lines.ShouldSay(InviteKind::Party, L"P2", 300));
}

TEST_CASE("a short helper run says nothing [ui][round14]")
{
    HelperRunSummary summary;
    summary.Start(5'000, 100, 1'000);
    CHECK_FALSE(summary.Stop(5'000 + HelperRunSummary::MinRunMs - 1, 101, 2'000).has_value());
    CHECK_FALSE(summary.Stop(999'999, 101, 2'000).has_value()); // not running any more
}

TEST_CASE("a helper run reports time, levels and zen once [ui][round14]")
{
    HelperRunSummary summary;
    summary.Start(10'000, 200, 5'000'000);
    const std::uint32_t length = (2 * 60 + 5) * 60'000 + 59'000; // 2 h 5 min 59 s
    const auto result = summary.Stop(10'000 + length, 203, 6'250'000);
    REQUIRE(result.has_value());
    CHECK(result->hours == 2);
    CHECK(result->minutes == 5);
    CHECK(result->levels == 3);
    CHECK(result->zen == 1'250'000);
    CHECK_FALSE(summary.Stop(10'000 + length, 203, 6'250'000).has_value());
}

TEST_CASE("a helper run across the tick wrap and a reset [ui][round14]")
{
    HelperRunSummary summary;
    summary.Start(0xFFFF'FFFFu - 30'000, 400, 1'000);
    const auto result = summary.Stop(90'000, 10, 700);
    REQUIRE(result.has_value());
    CHECK(result->hours == 0);
    CHECK(result->minutes == 2);
    CHECK(result->levels == 0);
    CHECK(result->zen == -300);
}

TEST_CASE("the zen change carries its sign [ui][round14]")
{
    CHECK(UI::Chat::SignedGroupedZen(1'250'000) == L"+1,250,000");
    CHECK(UI::Chat::SignedGroupedZen(-300) == L"-300");
    CHECK(UI::Chat::SignedGroupedZen(0) == L"0");
}
