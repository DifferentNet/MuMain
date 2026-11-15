#include <doctest.h>

#include "UI/Chat/PartyInviteCommand.h"

#include <string>

using UI::Chat::PartyCommandArgument;
using UI::Chat::SameCharacterName;

TEST_CASE("The party command alone has an empty argument")
{
    CHECK(PartyCommandArgument(L"/party", L"") == std::wstring_view(L""));
    CHECK(PartyCommandArgument(L"/PT", L"") == std::wstring_view(L""));
    CHECK(PartyCommandArgument(L"  /Party  ", L"") == std::wstring_view(L""));
}

TEST_CASE("The name after the command is trimmed")
{
    CHECK(PartyCommandArgument(L"/party Bob", L"") == std::wstring_view(L"Bob"));
    CHECK(PartyCommandArgument(L"/pt   Bob  ", L"") == std::wstring_view(L"Bob"));
}

TEST_CASE("The language's own word counts as the command, exactly")
{
    CHECK(PartyCommandArgument(L"/gruppa Bob", L"/gruppa") == std::wstring_view(L"Bob"));
    CHECK_FALSE(PartyCommandArgument(L"/gruppa Bob", L"").has_value());
}

TEST_CASE("Other text is not the party command")
{
    CHECK_FALSE(PartyCommandArgument(L"/partyBob", L"").has_value());
    CHECK_FALSE(PartyCommandArgument(L"/trade", L"").has_value());
    CHECK_FALSE(PartyCommandArgument(L"hello /party", L"").has_value());
    CHECK_FALSE(PartyCommandArgument(L"", L"").has_value());
}

TEST_CASE("Names match ignoring case but not length")
{
    CHECK(SameCharacterName(L"Bob", L"bOB"));
    CHECK(SameCharacterName(L"", L""));
    CHECK_FALSE(SameCharacterName(L"Bob", L"Bobby"));
    CHECK_FALSE(SameCharacterName(L"Bob", L"Rob"));
}
