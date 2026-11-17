#pragma once

#include <initializer_list>
#include <optional>
#include <string_view>

// The party invite command typed into the chat (Engine/Object/ZzzInterface.cpp CheckCommand): "/party", "/pt" or the
// language's own word, alone or followed by a player's name. The server takes an invite for any player it shows to
// the inviter, so the name picks one of the players in sight; without a name the selected player, then the one
// standing face to face, is invited. "/trade" takes a name the same way (CommandArgument). "/r" and "/reply" take
// the text to whisper back to the last player who whispered (ReplyCommandArgument).
namespace UI::Chat
{
// The text after the command word, spaces trimmed (empty when only the command was typed), or nothing when text does
// not start with one of the commands. The English commands match ignoring letter case; localizedCommand (the
// language's word, empty skips it) matches exactly.
std::optional<std::wstring_view> CommandArgument(std::wstring_view text, std::initializer_list<std::wstring_view> commands,
                                                 std::wstring_view localizedCommand);

// The text after the command word, spaces trimmed (empty when only the command was typed), or nothing when text is
// not the party command. localizedCommand is the language's word for it (I18N::Game::Party256); empty skips it.
std::optional<std::wstring_view> PartyCommandArgument(std::wstring_view text, std::wstring_view localizedCommand);

// The text after "/r" or "/reply", spaces trimmed (empty when only the command was typed), or nothing when text is
// not the reply command. Only the whole first word counts, so "/reset" or "/rift" are not replies.
std::optional<std::wstring_view> ReplyCommandArgument(std::wstring_view text);

// Character names compared as the game shows them, ignoring letter case.
bool SameCharacterName(std::wstring_view a, std::wstring_view b);
} // namespace UI::Chat
