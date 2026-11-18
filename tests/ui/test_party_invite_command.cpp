#include <doctest.h>

#include "UI/Chat/PartyInviteCommand.h"

#include <string>

using UI::Chat::CommandArgument;
using UI::Chat::PartyCommandArgument;
using UI::Chat::ReplyCommandArgument;
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

TEST_CASE("The reply command takes the rest of the line as the whisper")
{
    CHECK(ReplyCommandArgument(L"/r hello there") == std::wstring_view(L"hello there"));
    CHECK(ReplyCommandArgument(L"/REPLY  hi ") == std::wstring_view(L"hi"));
    CHECK(ReplyCommandArgument(L"/r") == std::wstring_view(L""));
}

TEST_CASE("Commands that only start like the reply command are not replies")
{
    CHECK_FALSE(ReplyCommandArgument(L"/reset").has_value());
    CHECK_FALSE(ReplyCommandArgument(L"/rift 2").has_value());
    CHECK_FALSE(ReplyCommandArgument(L"/replyhi").has_value());
    CHECK_FALSE(ReplyCommandArgument(L"r hello").has_value());
}

TEST_CASE("Names match ignoring case but not length")
{
    CHECK(SameCharacterName(L"Bob", L"bOB"));
    CHECK(SameCharacterName(L"", L""));
    CHECK_FALSE(SameCharacterName(L"Bob", L"Bobby"));
    CHECK_FALSE(SameCharacterName(L"Bob", L"Rob"));
}

TEST_CASE("The trade command takes a name the same way")
{
    CHECK(CommandArgument(L"/trade", { L"/trade" }, L"") == std::wstring_view(L""));
    CHECK(CommandArgument(L"/Trade  Bob ", { L"/trade" }, L"") == std::wstring_view(L"Bob"));
    CHECK(CommandArgument(L"/handel Bob", { L"/trade" }, L"/handel") == std::wstring_view(L"Bob"));
    CHECK(CommandArgument(L"/exchange Bob", { L"/trade", L"/exchange" }, L"") == std::wstring_view(L"Bob"));
    CHECK_FALSE(CommandArgument(L"/tradeBob", { L"/trade" }, L"").has_value());
    CHECK_FALSE(CommandArgument(L"/party Bob", { L"/trade" }, L"").has_value());
    CHECK_FALSE(CommandArgument(L"/trade", { L"" }, L"").has_value());
}

TEST_CASE("The old store command counts only as the whole first word")
{
    CHECK(CommandArgument(L"/purchase", { L"/purchase", L"/buy" }, L"/Kupit").has_value());
    CHECK(CommandArgument(L"/BUY Bob", { L"/purchase", L"/buy" }, L"/Kupit") == std::wstring_view(L"Bob"));
    CHECK(CommandArgument(L"/Kupit", { L"/purchase", L"/buy" }, L"/Kupit").has_value());
    CHECK_FALSE(CommandArgument(L"wtb sword /purchase 123456789012345678901234567890", { L"/purchase", L"/buy" }, L"/Kupit").has_value());
    CHECK_FALSE(CommandArgument(L"/purchases", { L"/purchase", L"/buy" }, L"").has_value());
    CHECK_FALSE(CommandArgument(L"/kupit", { L"/purchase", L"/buy" }, L"/Kupit").has_value());
}
