#include "doctest.h"

#include "Core/Utilities/Log/WideFormat.h"

#include <cstdarg>
#include <cwchar>
#include <iterator>
#include <string>

using Core::Log::FormatLogLine;
using Core::Log::NarrowSpecsForMusl;
using Core::Log::UnformattedMarker;
using Core::Log::WriteUnformatted;

namespace
{
int Format(wchar_t* buffer, std::size_t count, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = FormatLogLine(buffer, count, format, args);
    va_end(args);
    return written;
}
} // namespace

TEST_CASE("NarrowSpecsForMusl turns %hs into %s and keeps what stands between")
{
    CHECK(NarrowSpecsForMusl(L"%hs") == L"%s");
    CHECK(NarrowSpecsForMusl(L"> [RmlTheme] Saved theme '%hs' is unavailable; using '%hs'.\r\n")
          == L"> [RmlTheme] Saved theme '%s' is unavailable; using '%s'.\r\n");
    CHECK(NarrowSpecsForMusl(L"[%-8hs]") == L"[%-8s]");
    CHECK(NarrowSpecsForMusl(L"safe_cast<%.64hs>: %u") == L"safe_cast<%.64s>: %u");
    CHECK(NarrowSpecsForMusl(L"%*.*hs") == L"%*.*s");
    CHECK(NarrowSpecsForMusl(L"%1$hs %2$d") == L"%1$s %2$d");
}

TEST_CASE("NarrowSpecsForMusl leaves everything else as it was")
{
    // A plain percent sign followed by "hs", and one before a real %hs.
    CHECK(NarrowSpecsForMusl(L"100%%hs") == L"100%%hs");
    CHECK(NarrowSpecsForMusl(L"%%%hs") == L"%%%s");
    // Wide strings, other conversions with h, and no conversion at all.
    CHECK(NarrowSpecsForMusl(L"%ls %s %hd %hu %c %d%%") == L"%ls %s %hd %hu %c %d%%");
    CHECK(NarrowSpecsForMusl(L"no conversion") == L"no conversion");
    // A lone % at the end, or an h at the end, is copied as it is.
    CHECK(NarrowSpecsForMusl(L"50%") == L"50%");
    CHECK(NarrowSpecsForMusl(L"%h") == L"%h");
    CHECK(NarrowSpecsForMusl(L"") == L"");
    CHECK(NarrowSpecsForMusl(nullptr).empty());
}

TEST_CASE("FormatLogLine writes a %hs argument on this platform")
{
    wchar_t buffer[64];
    CHECK(Format(buffer, std::size(buffer), L"[%hs] %d %ls", "abc", 7, L"x") == 9);
    CHECK(std::wstring(buffer) == L"[abc] 7 x");
    CHECK(Format(buffer, std::size(buffer), L"[%-5hs]", "ab") == 7);
    CHECK(std::wstring(buffer) == L"[ab   ]");
    CHECK(Format(buffer, std::size(buffer), L"%.2hs", "abcdef") == 2);
    CHECK(std::wstring(buffer) == L"ab");
}

TEST_CASE("FormatLogLine always ends the line inside the buffer")
{
    wchar_t buffer[8];
    std::wmemset(buffer, L'x', std::size(buffer));
    Format(buffer, std::size(buffer), L"%hs", "a line much longer than eight characters");
    CHECK(std::wcslen(buffer) < std::size(buffer));

    wchar_t one[1] = {L'x'};
    Format(one, std::size(one), L"%d", 5);
    CHECK(one[0] == L'\0');
}

TEST_CASE("A refused format becomes the marker and the format itself")
{
    wchar_t buffer[64];
    WriteUnformatted(buffer, std::size(buffer), L"> [RmlTheme] %hs.\r\n");
    CHECK(std::wstring(buffer) == std::wstring(UnformattedMarker) + L"> [RmlTheme] %hs.\r\n");

    // Cut to fit, and ended.
    wchar_t small[6];
    WriteUnformatted(small, std::size(small), L"anything");
    CHECK(std::wstring(small) == std::wstring(UnformattedMarker).substr(0, 5));

    WriteUnformatted(buffer, std::size(buffer), nullptr);
    CHECK(std::wstring(buffer) == UnformattedMarker);
}
