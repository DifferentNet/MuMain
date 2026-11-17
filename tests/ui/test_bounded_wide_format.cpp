#include "Core/Text/MuSwprintf.h"

#include <doctest.h>

#include <cwchar>
#include <string>

// mu_swprintf into an array never writes past it (a long translation is cut); into a pointer it writes all.
namespace
{
struct TwoRows
{
    wchar_t first[100];
    wchar_t second[100];
};

const std::wstring kLongText(120, L'x');
} // namespace

TEST_CASE("a text longer than the array is cut and the next row keeps its text [ui][format]")
{
    TwoRows rows{};
    std::wcscpy(rows.second, L"next row");

    const int written = mu_swprintf(rows.first, L"%ls", kLongText.c_str());

    CHECK(std::wstring(rows.second) == L"next row");
#ifdef _MSC_VER
    // The web build's std::swprintf reports a cut text as -1; the PC build returns the cut length.
    CHECK(written == 99);
    CHECK(std::wcslen(rows.first) == 99);
#else
    (void)written;
#endif
}

TEST_CASE("a text that fits is written whole and its length returned [ui][format]")
{
    wchar_t text[16] = {};
    const int written = mu_swprintf(text, L"%ls %d", L"Level", 400);

    CHECK(written == 9);
    CHECK(std::wstring(text) == L"Level 400");
}

#ifdef _MSC_VER
TEST_CASE("a text exactly one character too long loses only that character [ui][format]")
{
    wchar_t text[4] = {};
    const int written = mu_swprintf(text, L"%ls", L"abcd");

    CHECK(written == 3);
    CHECK(std::wstring(text) == L"abc");
}
#endif

TEST_CASE("a pointer destination is written in full as before [ui][format]")
{
    wchar_t buffer[200] = {};
    wchar_t* destination = buffer;
    const int written = mu_swprintf(destination, L"%ls", kLongText.c_str());

    CHECK(written == 120);
    CHECK(std::wstring(buffer) == kLongText);
}
