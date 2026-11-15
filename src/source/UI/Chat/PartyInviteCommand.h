#pragma once

#include <optional>
#include <string_view>

// The party invite command typed into the chat (Engine/Object/ZzzInterface.cpp CheckCommand): "/party", "/pt" or the
// language's own word, alone or followed by a player's name. The server takes an invite for any player it shows to
// the inviter, so the name picks one of the players in sight; without a name the selected player, then the one
// standing face to face, is invited.
namespace UI::Chat
{
// The text after the command word, spaces trimmed (empty when only the command was typed), or nothing when text is
// not the party command. localizedCommand is the language's word for it (I18N::Game::Party256); empty skips it.
std::optional<std::wstring_view> PartyCommandArgument(std::wstring_view text, std::wstring_view localizedCommand);

// Character names compared as the game shows them, ignoring letter case.
bool SameCharacterName(std::wstring_view a, std::wstring_view b);
} // namespace UI::Chat
