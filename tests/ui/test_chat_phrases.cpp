#include <doctest.h>

#include "UI/Chat/ChatPhrases.h"

#include <optional>
#include <string>
#include <string_view>

namespace Phrases = UI::Chat::Phrases;

namespace
{
const wchar_t* Russian(int id)
{
    switch (id)
    {
    case 2:
        return L"Спасибо!"; // Спасибо!
    case 5:
        return L"";
    default:
        return id == 12 ? L"Bye in Russian" : nullptr;
    }
}
} // namespace

TEST_CASE("a phrase goes as its token and the English text [ui][chat-phrases]")
{
    CHECK(Phrases::Compose(1) == L"[#01] Hello!");
    CHECK(Phrases::Compose(2) == L"[#02] Thanks!");
    CHECK(Phrases::Compose(12) == L"[#12] Bye");
    CHECK(Phrases::Compose(0).empty());
    CHECK(Phrases::Compose(Phrases::Count + 1).empty());
    CHECK(Phrases::EnglishText(3) == std::wstring_view(L"Invite me to the party"));
}

TEST_CASE("every phrase reads back as its own id and has text [ui][chat-phrases]")
{
    for (int id = 1; id <= Phrases::Count; ++id)
    {
        const std::wstring line = Phrases::Compose(id);
        CHECK_FALSE(Phrases::EnglishText(id).empty());
        CHECK(Phrases::IdOf(line) == std::optional<int>(id));
        // The token never starts with a prefix the chat reads (~ @ $ # ! /).
        CHECK(line[0] == L'[');
    }
}

TEST_CASE("only a well-formed token with a known id is a phrase [ui][chat-phrases]")
{
    CHECK(Phrases::IdOf(L"[#07]") == std::optional<int>(7));
    CHECK(Phrases::IdOf(L"[#07] anything typed") == std::optional<int>(7));
    CHECK_FALSE(Phrases::IdOf(L"").has_value());
    CHECK_FALSE(Phrases::IdOf(L"Thanks!").has_value());
    CHECK_FALSE(Phrases::IdOf(L"[#00] Nothing").has_value());
    CHECK_FALSE(Phrases::IdOf(L"[#13] From a newer client").has_value());
    CHECK_FALSE(Phrases::IdOf(L"[#7] Short").has_value());
    CHECK_FALSE(Phrases::IdOf(L"[#07]x").has_value());
    CHECK_FALSE(Phrases::IdOf(L"[#0a] Letter").has_value());
    CHECK_FALSE(Phrases::IdOf(L"#[07] Swapped").has_value());
    CHECK_FALSE(Phrases::IdOf(L" [#07] Space first").has_value());
}

TEST_CASE("a known phrase shows in the reader's language, anything else as it came [ui][chat-phrases]")
{
    CHECK(Phrases::TryTranslate(L"[#02] Thanks!", Russian) == std::optional<std::wstring>(L"Спасибо!"));
    CHECK(Phrases::TryTranslate(L"[#12] Bye", Russian) == std::optional<std::wstring>(L"Bye in Russian"));
    // No own text, or an empty one: the English line stays.
    CHECK_FALSE(Phrases::TryTranslate(L"[#03] Invite me to the party", Russian).has_value());
    CHECK_FALSE(Phrases::TryTranslate(L"[#05] Help!", Russian).has_value());
    CHECK_FALSE(Phrases::TryTranslate(L"[#02] Thanks!", nullptr).has_value());
    CHECK_FALSE(Phrases::TryTranslate(L"[#99] Unknown", Russian).has_value());
    CHECK_FALSE(Phrases::TryTranslate(L"Thanks!", Russian).has_value());
}
