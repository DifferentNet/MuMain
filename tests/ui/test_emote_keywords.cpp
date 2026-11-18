#include <doctest.h>

#include "UI/Chat/EmoteKeywords.h"

using UI::Chat::EmoteKeywords::ContainsKeyword;
using UI::Chat::EmoteKeywords::FirstMatch;
using UI::Chat::EmoteKeywords::FoldCase;
using UI::Chat::EmoteKeywords::Group;

namespace
{
const wchar_t* const Greeting[] = {L"Привет", L"прив", L"Спасибо"};
const wchar_t* const Refuse[] = {L"нет", L"не надо"};
const wchar_t* const Laugh[] = {L"хаха", L"^^"};
const wchar_t* const English[] = {L"Hi", L"Not"};
const wchar_t* const Beckon[] = {L"ко мне"};

int Match(const wchar_t* line)
{
    const Group groups[] = {{Greeting}, {Refuse}, {Laugh, true}, {English}, {Beckon}};
    return FirstMatch(line, groups);
}
} // namespace

TEST_CASE("Case folding covers Latin, Latin-1, Polish and Cyrillic letters")
{
    CHECK(FoldCase(std::wstring_view(L"HELLO")) == L"hello");
    CHECK(FoldCase(std::wstring_view(L"ПРИВЕТ Ёлка")) == L"привет ёлка");
    CHECK(FoldCase(std::wstring_view(L"ŁÓDŹ ŻÓŁW")) == L"łódź żółw");
    CHECK(FoldCase(std::wstring_view(L"ҐІЇЄ")) == L"ґіїє");
    CHECK(FoldCase(std::wstring_view(L"^^ T_T 123")) == L"^^ t_t 123");
}

TEST_CASE("Cyrillic keywords match in any letter case")
{
    CHECK(Match(L"привет всем") == 0);
    CHECK(Match(L"СПАСИБО!") == 0);
}

TEST_CASE("Other keywords match as before: case-sensitive, anywhere")
{
    CHECK(Match(L"Hi there") == 3);
    CHECK(Match(L"hi there") == -1);
    CHECK(Match(L"not now") == -1);
    CHECK(Match(L"Nothing") == 3);
}

TEST_CASE("A keyword of several words must start a word")
{
    CHECK(Match(L"мне надо в лорен") == -1);
    CHECK(Match(L"молоко мне") == -1);
    CHECK(Match(L"не надо") == 1);
    CHECK(Match(L"иди ко мне!") == 4);
}

TEST_CASE("Short keywords must stand as whole words")
{
    // "нет" inside other words.
    CHECK(Match(L"монеты нету") == -1);
    CHECK(Match(L"нет, спасибо не надо") == 0);
    CHECK(Match(L"нет.") == 1);
    CHECK(Match(L"(прив)") == 0);
    CHECK(Match(L"я не знаю где это") == -1);
}

TEST_CASE("Longer keywords and laughter match inside words")
{
    CHECK(Match(L"приветик") == 0);
    CHECK(Match(L"ахахахах") == 2);
    CHECK(Match(L"ok ^^") == 2);
}

TEST_CASE("Keywords of other scripts match anywhere")
{
    const wchar_t* const thanks[] = {L"ありがとう"};
    const Group groups[] = {{thanks}};
    CHECK(FirstMatch(L"今日はありがとう", groups) == 0);
    CHECK(UI::Chat::EmoteKeywords::HasCyrillic(L"не надо"));
    CHECK_FALSE(UI::Chat::EmoteKeywords::HasCyrillic(L"Not"));
    CHECK(ContainsKeyword(L"t_t", L"t_t", false));
    CHECK_FALSE(ContainsKeyword(L"abc", L"", false));
}

TEST_CASE("A null keyword is skipped and an empty line matches nothing")
{
    const wchar_t* const withNull[] = {nullptr, L"yes"};
    const Group groups[] = {{withNull}};
    CHECK(FirstMatch(L"yes", groups) == 0);
    CHECK(FirstMatch(L"", groups) == -1);
}
