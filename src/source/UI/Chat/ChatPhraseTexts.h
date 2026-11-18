// The quick chat phrases' texts in the player's own language (ChatPhrase01..12 in Localization/Game.*.resx), and the
// chat lines that show them in place of the phrase token (UI/Chat/ChatPhrases).
#pragma once

#include <cstddef>

namespace UI::Chat::PhraseTexts
{
// The phrase's text in the current language, or null for an id out of range.
[[nodiscard]] const wchar_t* OwnText(int id);

// Puts the own-language text in place of a phrase line ("[#07] Thanks!" -> "Спасибо!"), cut to the buffer.
// @return whether the line was a known phrase.
bool ShowInOwnLanguage(wchar_t* text, std::size_t capacity);
} // namespace UI::Chat::PhraseTexts
