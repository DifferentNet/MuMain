// Quick chat phrases (pad keyboard's phrase page, a phone's chips over the chat bar) that each reader sees in
// their own language. A phrase travels as ordinary chat text after the channel prefix: a token and the English
// text, "[#07] Thanks!". A client that knows the id shows its own language's text instead (UI/Chat/ChatPhraseTexts);
// an old client, an unknown id or the control socket's log keep the English line as it came. The token starts
// with '[' so it never meets the prefixes the chat reads (~ @ @@ $ # ! /). No drawing, no I18N here.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace UI::Chat::Phrases
{
// Ids run 1..Count.
inline constexpr int Count = 12;

// The id's English text ("Thanks!"), or empty for an id out of range.
[[nodiscard]] std::wstring_view EnglishText(int id);

// The line to send for the id: "[#07] Thanks!"; empty for an id out of range.
[[nodiscard]] std::wstring Compose(int id);

// The id of a line that starts with a known token ("[#07]" followed by a space or the end); nothing for any other
// line, an id out of range among them. What follows the token is not checked: a typed fake shows as the phrase.
[[nodiscard]] std::optional<int> IdOf(std::wstring_view text);

// The reader's text for an id (an I18N slot), or null when it has none.
using OwnTextOf = const wchar_t* (*)(int id);

// The line in the reader's language when it is a known phrase; nothing otherwise (show it as it came). A phrase
// whose own text is missing or empty is not translated either.
[[nodiscard]] std::optional<std::wstring> TryTranslate(std::wstring_view text, OwnTextOf ownText);
} // namespace UI::Chat::Phrases
