#include <doctest.h>

#include "UI/Chat/ChatChannelCycle.h"

namespace Cycle = UI::Chat::ChannelCycle;

TEST_CASE("in every group the click goes All, Party, Guild, Gens and round")
{
    CHECK(Cycle::NextChannel(Cycle::All, true, true, true) == Cycle::Party);
    CHECK(Cycle::NextChannel(Cycle::Party, true, true, true) == Cycle::Guild);
    CHECK(Cycle::NextChannel(Cycle::Guild, true, true, true) == Cycle::Gens);
    CHECK(Cycle::NextChannel(Cycle::Gens, true, true, true) == Cycle::All);
}

TEST_CASE("in no group the click stays on All")
{
    CHECK(Cycle::NextChannel(Cycle::All, false, false, false) == Cycle::All);
}

TEST_CASE("the groups the hero is not in are passed by")
{
    // No party: All -> Guild -> Gens -> All.
    CHECK(Cycle::NextChannel(Cycle::All, false, true, true) == Cycle::Guild);
    CHECK(Cycle::NextChannel(Cycle::Guild, false, true, true) == Cycle::Gens);
    // Party only: All <-> Party.
    CHECK(Cycle::NextChannel(Cycle::All, true, false, false) == Cycle::Party);
    CHECK(Cycle::NextChannel(Cycle::Party, true, false, false) == Cycle::All);
    // Gens only: All <-> Gens.
    CHECK(Cycle::NextChannel(Cycle::All, false, false, true) == Cycle::Gens);
    CHECK(Cycle::NextChannel(Cycle::Gens, false, false, true) == Cycle::All);
    // Party and gens: the guild is passed by.
    CHECK(Cycle::NextChannel(Cycle::Party, true, false, true) == Cycle::Gens);
}

TEST_CASE("from a channel the hero left the click goes to All")
{
    // He left the party while its channel was chosen; a guild would come next, yet All does.
    CHECK(Cycle::NextChannel(Cycle::Party, false, true, true) == Cycle::All);
    CHECK(Cycle::NextChannel(Cycle::Guild, true, false, true) == Cycle::All);
    CHECK(Cycle::NextChannel(Cycle::Gens, false, false, false) == Cycle::All);
}

TEST_CASE("a channel out of range goes to All")
{
    CHECK(Cycle::NextChannel(-1, true, true, true) == Cycle::All);
    CHECK(Cycle::NextChannel(Cycle::Count, true, true, true) == Cycle::All);
    CHECK(Cycle::NextChannel(99, false, false, false) == Cycle::All);
}

TEST_CASE("which channels are open")
{
    CHECK(Cycle::IsOpen(Cycle::All, false, false, false));
    CHECK(Cycle::IsOpen(Cycle::Party, true, false, false));
    CHECK_FALSE(Cycle::IsOpen(Cycle::Party, false, true, true));
    CHECK(Cycle::IsOpen(Cycle::Guild, false, true, false));
    CHECK_FALSE(Cycle::IsOpen(Cycle::Guild, true, false, true));
    CHECK(Cycle::IsOpen(Cycle::Gens, false, false, true));
    CHECK_FALSE(Cycle::IsOpen(Cycle::Gens, true, true, false));
    CHECK_FALSE(Cycle::IsOpen(-1, true, true, true));
    CHECK_FALSE(Cycle::IsOpen(Cycle::Count, true, true, true));
}
