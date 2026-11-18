#pragma once

#include <span>
#include <string>
#include <string_view>

// The emote a public chat line plays (UI/Chat/ChatInput.cpp): the first group with a keyword in the line wins.
// A keyword with Cyrillic letters matches in any letter case; when short (MaxWholeWordLength letters or fewer) it must
// stand as a whole word, so "не" no longer fires inside "меня", and one of several words ("не надо") must start a
// word, so it does not fire inside "мне надо". A group may allow keywords anywhere (laughter: "хахаха" holds "хаха").
// Every other keyword (Latin, smileys, CJK text) matches as it always did: case-sensitive, anywhere in the line.
namespace UI::Chat::EmoteKeywords
{
inline constexpr std::size_t MaxWholeWordLength = 4;

struct Group
{
    std::span<const wchar_t* const> keywords;
    // Short keywords match inside words too.
    bool anywhere = false;
};

// Lower case for the letters typed here (Latin, Latin-1, Latin Extended-A, Cyrillic); other characters stay.
[[nodiscard]] wchar_t FoldCase(wchar_t c);
[[nodiscard]] std::wstring FoldCase(std::wstring_view text);

// Letters and digits of the scripts FoldCase knows: what a whole word is made of.
[[nodiscard]] bool IsWordCharacter(wchar_t c);

// Whether the keyword has a Cyrillic letter: only those get case folding and the word rules.
[[nodiscard]] bool HasCyrillic(std::wstring_view keyword);

// Whether the keyword is in the text by the word rules; both already folded. An empty keyword is never found.
[[nodiscard]] bool ContainsKeyword(std::wstring_view foldedText, std::wstring_view foldedKeyword, bool anywhere);

// The first group with a keyword in the text, or -1 when none has one. Null and empty keywords are skipped.
[[nodiscard]] int FirstMatch(std::wstring_view text, std::span<const Group> groups);
} // namespace UI::Chat::EmoteKeywords
