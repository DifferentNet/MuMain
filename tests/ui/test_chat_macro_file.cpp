#include <doctest.h>

#include "UI/Chat/ChatMacroFile.h"

#include <string>

using namespace UI::Chat::Macros;

TEST_CASE("each line of the old macro file is one slot and spaces inside stay")
{
    const MacroSlots slots = ParseLegacyFile(L"hello world\n\n  two  spaces \nlast\n");

    CHECK(slots[0] == L"hello world");
    CHECK(slots[1].empty());
    CHECK(slots[2] == L"  two  spaces ");
    CHECK(slots[3] == L"last");
    for (int i = 4; i < kSlotCount; ++i)
        CHECK(slots[i].empty());
}

TEST_CASE("a macro only in slot 3 comes back in slot 3")
{
    const MacroSlots slots = ParseLegacyFile(L"\n\nпривет всем\n\n\n\n\n\n\n\n");

    CHECK(slots[0].empty());
    CHECK(slots[1].empty());
    CHECK(slots[2] == L"привет всем");
}

TEST_CASE("CRLF line ends, a missing last line end and lines past ten")
{
    std::wstring text;
    for (int i = 0; i < 12; ++i)
        text += L"m" + std::to_wstring(i) + L"\r\n";
    text += L"tail";
    const MacroSlots slots = ParseLegacyFile(text);

    CHECK(slots[0] == L"m0");
    CHECK(slots[9] == L"m9");

    const MacroSlots shortFile = ParseLegacyFile(L"one\r\ntwo");
    CHECK(shortFile[0] == L"one");
    CHECK(shortFile[1] == L"two");
}

TEST_CASE("an empty file gives empty slots and a long line is cut")
{
    CHECK(ParseLegacyFile(L"")[0].empty());

    const MacroSlots slots = ParseLegacyFile(std::wstring(400, L'a'));
    CHECK(slots[0].size() == kMaxLength);
}

TEST_CASE("slot keys run 1..9 then 0")
{
    CHECK(SlotKeyNumber(0) == 1);
    CHECK(SlotKeyNumber(8) == 9);
    CHECK(SlotKeyNumber(9) == 0);
}

TEST_CASE("ini encoding keeps Russian, spaces at the ends, quotes and percent signs")
{
    const std::wstring samples[] = {
        L"привет всем",
        L"  two spaces  ",
        L"\"quoted\"",
        L"100% win %41",
        L"",
        L"mix: Ünïcödé 日本 \U0001F600",
    };
    for (const std::wstring& text : samples)
    {
        const std::wstring encoded = EncodeForIni(text);
        for (const wchar_t c : encoded)
        {
            CHECK(c >= 0x21);
            CHECK(c <= 0x7E);
            CHECK(c != L'"');
        }
        CHECK(DecodeFromIni(encoded) == text);
    }
    CHECK(EncodeForIni(L"").empty());
    CHECK(EncodeForIni(L" a%\"") == L"%20a%25%22");
    CHECK(EncodeForIni(L"п") == L"%D0%BF");
}

TEST_CASE("a macro saved before the encoding reads the same")
{
    CHECK(DecodeFromIni(L"hello world") == L"hello world");
    CHECK(DecodeFromIni(L"привет") == L"привет");
    CHECK(DecodeFromIni(L"50% off") == L"50% off");
    CHECK(DecodeFromIni(L"%") == L"%");
    CHECK(DecodeFromIni(L"%D0%BF%D1%80%D0%B8") == L"при");
}
