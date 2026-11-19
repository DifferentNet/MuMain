#include <doctest.h>

#include "UI/HUD/WarpListMarks.h"
#include "UI/Inventory/TradeOfferChange.h"

#include <cstdint>
#include <vector>

using UI::MoveCommand::Marks::DeathSpot;
using UI::MoveCommand::Marks::NearestRow;
using UI::MoveCommand::Marks::RowSpot;

namespace
{
constexpr int kLorencia = 0;
constexpr int kDungeon = 1;
constexpr int kLostTower = 4;
constexpr std::uint32_t kStartMs = 100000;

// Lorencia, Dungeon 1-2, Lost Tower 1-3 and a row whose gate the client does not know.
std::vector<RowSpot> SampleRows()
{
    return {
        {kLorencia, 130, 120}, {kDungeon, 108, 247}, {kDungeon, 233, 234}, {kLostTower, 208, 77},
        {kLostTower, 241, 176}, {kLostTower, 87, 150}, {-1, 0, 0},
    };
}
} // namespace

TEST_CASE("warp list: the row of the map the hero is on [ui][round20]")
{
    const auto rows = SampleRows();
    CHECK(NearestRow(rows, kLorencia, 10, 10) == 0);
    // Several floors on one world: the floor nearest the hero.
    CHECK(NearestRow(rows, kLostTower, 90, 140) == 5);
    CHECK(NearestRow(rows, kLostTower, 230, 180) == 4);
    CHECK(NearestRow(rows, kDungeon, 230, 230) == 2);
}

TEST_CASE("warp list: a world with no row, or an unknown one, gives no mark [ui][round20]")
{
    const auto rows = SampleRows();
    CHECK(NearestRow(rows, 33, 100, 100) == -1);
    CHECK(NearestRow(rows, -1, 0, 0) == -1);
    CHECK(NearestRow({}, kLorencia, 0, 0) == -1);
}

TEST_CASE("warp list: the death spot lasts ten minutes [ui][round20]")
{
    DeathSpot spot;
    int world = -1;
    int x = 0;
    int y = 0;
    CHECK_FALSE(spot.Get(kStartMs, world, x, y));

    spot.NoteDeath(kLostTower, 87, 150, kStartMs);
    REQUIRE(spot.Get(kStartMs + DeathSpot::KeepMs - 1, world, x, y));
    CHECK(world == kLostTower);
    CHECK(x == 87);
    CHECK(y == 150);
    CHECK_FALSE(spot.Get(kStartMs + DeathSpot::KeepMs, world, x, y));
}

TEST_CASE("warp list: the respawn keeps the death spot, the next warp clears it [ui][round20]")
{
    DeathSpot spot;
    int world = -1;
    int x = 0;
    int y = 0;
    spot.NoteDeath(kLostTower, 87, 150, kStartMs);
    // A warp before the revival (none should come) changes nothing; the respawn keeps the spot.
    spot.NoteWarp();
    spot.NoteRevival();
    CHECK(spot.Get(kStartMs + 10000, world, x, y));

    spot.NoteWarp();
    CHECK_FALSE(spot.Get(kStartMs + 10001, world, x, y));
}

TEST_CASE("warp list: a second respawn without a death is a server move and clears the spot [ui][round20]")
{
    DeathSpot spot;
    int world = -1;
    int x = 0;
    int y = 0;
    spot.NoteDeath(kLostTower, 87, 150, kStartMs);
    spot.NoteRevival();
    CHECK(spot.Get(kStartMs + 1000, world, x, y));
    spot.NoteRevival();
    CHECK_FALSE(spot.Get(kStartMs + 1001, world, x, y));
}

TEST_CASE("warp list: a new death replaces the old spot, a new login forgets it [ui][round20]")
{
    DeathSpot spot;
    int world = -1;
    int x = 0;
    int y = 0;
    spot.NoteDeath(kLostTower, 87, 150, kStartMs);
    spot.NoteRevival();
    spot.NoteDeath(kDungeon, 233, 234, kStartMs + 60000);
    REQUIRE(spot.Get(kStartMs + 60001, world, x, y));
    CHECK(world == kDungeon);
    // The respawn after the second death is a first one again: the spot stays.
    spot.NoteRevival();
    CHECK(spot.Get(kStartMs + 63000, world, x, y));

    spot.Clear();
    CHECK_FALSE(spot.Get(kStartMs + 63001, world, x, y));
}

TEST_CASE("trade: a trade closed without the server's exit drops its waiting lines [ui][round20]")
{
    UI::Trade::PendingRemovals pending;
    pending.Hold(100, 9, L"took out", 1'000);
    // The window closed (a warp): ProcessClosing() clears what still waits.
    pending.Clear();
    CHECK(pending.Empty());
    // The next trade hears nothing of it.
    CHECK(pending.TakeDue(1'000 + UI::Trade::PendingRemovals::HoldMs).empty());
}
