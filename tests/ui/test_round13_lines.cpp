#include <doctest.h>

#include "Scenes/CharacterPreselect.h"
#include "UI/Chat/PetLifeWarning.h"
#include "UI/Chat/SeenSkills.h"
#include "UI/Chat/SkillCostNotice.h"
#include "UI/Chat/WarpUnlocks.h"
#include "UI/Inventory/TradeOfferChange.h"

#include <string>
#include <vector>

using UI::Chat::PetLifeWarning;
using UI::Chat::SeenSkills;
namespace Warp = UI::Chat::WarpUnlocks;

TEST_CASE("trade says only a lowered zen offer [ui][round13]")
{
    CHECK(UI::Trade::IsZenLowered(10'000'000, 1'000'000));
    CHECK(UI::Trade::IsZenLowered(500, 0));
    // The first amount of a trade and a rise say nothing.
    CHECK_FALSE(UI::Trade::IsZenLowered(0, 1'000'000));
    CHECK_FALSE(UI::Trade::IsZenLowered(1'000, 2'000));
    CHECK_FALSE(UI::Trade::IsZenLowered(1'000, 1'000));
    CHECK_FALSE(UI::Trade::IsZenLowered(0, 0));
}

TEST_CASE("pet life warns once at a quarter and once at a tenth [ui][round13]")
{
    PetLifeWarning warning;
    CHECK(warning.Note(255, 200) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(200, 64) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(64, 63) == PetLifeWarning::Say::Quarter);
    CHECK(warning.Note(63, 40) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(40, 26) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(26, 25) == PetLifeWarning::Say::Tenth);
    CHECK(warning.Note(25, 10) == PetLifeWarning::Say::Nothing);
    // At 0 the "broken" line speaks.
    CHECK(warning.Note(10, 0) == PetLifeWarning::Say::Nothing);
}

TEST_CASE("pet life: a big drop says only the tenth [ui][round13]")
{
    PetLifeWarning warning;
    CHECK(warning.Note(255, 20) == PetLifeWarning::Say::Tenth);
    CHECK(warning.Note(20, 15) == PetLifeWarning::Say::Nothing);
}

TEST_CASE("pet life is armed again after it rose above the threshold [ui][round13]")
{
    PetLifeWarning warning;
    CHECK(warning.Note(70, 60) == PetLifeWarning::Say::Quarter);
    CHECK(warning.Note(60, 20) == PetLifeWarning::Say::Tenth);
    // Healed above a tenth only: the tenth comes again, the quarter not.
    CHECK(warning.Note(20, 40) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(40, 30) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(30, 24) == PetLifeWarning::Say::Tenth);
    // Repaired: both come again.
    CHECK(warning.Note(24, 255) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(255, 50) == PetLifeWarning::Say::Quarter);
    warning.Reset();
    CHECK(warning.Note(50, 49) == PetLifeWarning::Say::Quarter);
}

TEST_CASE("trade: an item moved inside the partner's box says nothing [ui][round13]")
{
    UI::Trade::PendingRemovals pending;
    pending.Hold(100, 9, L"took out", 1'000);
    // Gone and added again within the hold: a move.
    CHECK(pending.CancelOn(100, 9));
    CHECK(pending.TakeDue(2'000).empty());
    CHECK(pending.Empty());
}

TEST_CASE("trade: an item really taken out is said after the hold [ui][round13]")
{
    UI::Trade::PendingRemovals pending;
    pending.Hold(100, 9, L"first", 1'000);
    pending.Hold(200, 0, L"second", 1'100);
    // Another item (or the same type at another level) coming in does not cancel it.
    CHECK_FALSE(pending.CancelOn(100, 8));
    CHECK_FALSE(pending.CancelOn(300, 9));
    CHECK(pending.TakeDue(1'299).empty());
    const auto due = pending.TakeDue(1'300);
    REQUIRE(due.size() == 1);
    CHECK(due[0] == L"first");
    const auto rest = pending.TakeDue(1'400);
    REQUIRE(rest.size() == 1);
    CHECK(rest[0] == L"second");
    CHECK(pending.Empty());
}

TEST_CASE("pet life: another pet of the same kind is warned about again [ui][round13]")
{
    PetLifeWarning warning;
    CHECK(warning.Note(30, 20) == PetLifeWarning::Say::Tenth);
    CHECK(warning.Note(20, 18) == PetLifeWarning::Say::Nothing);
    // The next update does not go on from 18: a swapped pet, low too.
    CHECK(warning.Note(22, 21) == PetLifeWarning::Say::Tenth);
    CHECK(warning.Note(21, 19) == PetLifeWarning::Say::Nothing);
}

TEST_CASE("pet life: the first value and a rise say nothing [ui][round13]")
{
    PetLifeWarning warning;
    CHECK(warning.Note(0, 30) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(20, 30) == PetLifeWarning::Say::Nothing);
    CHECK(warning.Note(30, 30) == PetLifeWarning::Say::Nothing);
}

TEST_CASE("a skill is new only once per character [ui][round13]")
{
    SeenSkills seen;
    seen.BeginList(L"Hero");
    seen.NoteKnown(11);
    seen.NoteKnown(19);
    CHECK_FALSE(seen.NoteAdded(11));
    CHECK(seen.NoteAdded(41));
    // A weapon skill taken off and put on again, or the full list sent again.
    CHECK_FALSE(seen.NoteAdded(41));
    seen.BeginList(L"Hero");
    seen.NoteKnown(11);
    CHECK_FALSE(seen.NoteAdded(41));
    CHECK_FALSE(seen.NoteAdded(0));
    // Another character starts afresh.
    seen.BeginList(L"Other");
    CHECK(seen.NoteAdded(41));
}

TEST_CASE("warp destinations opened by a level-up [ui][round13]")
{
    const std::vector<Warp::Destination> list{
        {L"Lorencia", 10}, {L"Noria", 10}, {L"Devias", 20}, {L"Dungeon", 30}, {L"Dungeon", 40},
        {L"Lost Tower", 50}, {L"Atlans", 70}, {L"Tarkan", 140}, {L"Icarus", 170}, {L"Aida", 400},
    };

    CHECK(Warp::NewlyReached(list, false, 19, 20) == std::vector<std::wstring>{L"Devias"});
    CHECK(Warp::NewlyReached(list, false, 20, 21).empty());
    CHECK(Warp::NewlyReached(list, false, 9, 10) == std::vector<std::wstring>{L"Lorencia", L"Noria"});
    // The second floor of a map already open says nothing.
    CHECK(Warp::NewlyReached(list, false, 39, 40).empty());
    // At most two names.
    CHECK(Warp::NewlyReached(list, false, 0, 50).size() == 2);
    CHECK(Warp::NewlyReached(list, false, 21, 19).empty());

    // The Dark classes: two thirds of the listed level, except 400.
    CHECK(Warp::RequiredLevel(70, true) == 46);
    CHECK(Warp::RequiredLevel(70, false) == 70);
    CHECK(Warp::RequiredLevel(400, true) == 400);
    CHECK(Warp::NewlyReached(list, true, 45, 46) == std::vector<std::wstring>{L"Atlans"});
    CHECK(Warp::NewlyReached(list, true, 399, 400) == std::vector<std::wstring>{L"Aida"});

    CHECK(Warp::Join({L"Lorencia", L"Noria"}) == L"Lorencia, Noria");
    CHECK(Warp::Join({}).empty());
}

TEST_CASE("character select preselects the last played or the only character [ui][round13]")
{
    CHECK(Scenes::PickPreselectSlot({L"", L"Knight", L"", L"Elf", L""}, L"Elf") == 3);
    CHECK(Scenes::PickPreselectSlot({L"", L"Knight", L"", L"Elf", L""}, L"Gone") == -1);
    CHECK(Scenes::PickPreselectSlot({L"", L"Knight", L"", L"Elf", L""}, L"") == -1);
    CHECK(Scenes::PickPreselectSlot({L"", L"", L"Only", L"", L""}, L"Gone") == 2);
    CHECK(Scenes::PickPreselectSlot({L"", L"", L"", L"", L""}, L"Elf") == -1);
    CHECK(Scenes::PickPreselectSlot({}, L"Elf") == -1);
}

TEST_CASE("short of mana is said at most once in a while [ui][round13]")
{
    UI::Chat::SkillCost::SayGate gate;
    CHECK(gate.Allow(1'000));
    CHECK_FALSE(gate.Allow(1'500));
    CHECK_FALSE(gate.Allow(1'000 + UI::Chat::SkillCost::SayGate::SayEveryMs - 1));
    CHECK(gate.Allow(1'000 + UI::Chat::SkillCost::SayGate::SayEveryMs));
    // GetTickCount wraps around.
    UI::Chat::SkillCost::SayGate wrapping;
    CHECK(wrapping.Allow(0xFFFF'FF00u));
    CHECK_FALSE(wrapping.Allow(0x0000'0010u));
}
