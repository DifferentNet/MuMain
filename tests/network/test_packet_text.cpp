#include "Network/Server/PacketText.h"

#include "doctest.h"

#include <string>

using Network::PacketText::CompleteUtf8Bytes;

TEST_CASE("packet text keeps whole UTF-8 text")
{
    CHECK(CompleteUtf8Bytes("") == 0);
    CHECK(CompleteUtf8Bytes("hello") == 5);
    CHECK(CompleteUtf8Bytes("\xD0\x9F\xD1\x80") == 4); // two Cyrillic letters
    CHECK(CompleteUtf8Bytes("a\xE2\x82\xAC") == 4);    // a three-byte character at the end
    CHECK(CompleteUtf8Bytes("\xF0\x9F\x98\x80") == 4); // a four-byte character
}

TEST_CASE("packet text drops a character cut off at the end")
{
    CHECK(CompleteUtf8Bytes("\xD0\x9F\xD1") == 2);  // lead byte without its continuation
    CHECK(CompleteUtf8Bytes("a\xE2\x82") == 1);     // three-byte character missing one byte
    CHECK(CompleteUtf8Bytes("a\xF0\x9F\x98") == 1); // four-byte character missing one byte
}

TEST_CASE("packet text stops at the first zero byte")
{
    const std::string bytes("ab\0cd", 5);
    CHECK(CompleteUtf8Bytes(bytes) == 2);
    const std::string cut("\xD0\x9F\xD1\0\x80", 5);
    CHECK(CompleteUtf8Bytes(cut) == 2);
}

TEST_CASE("packet text leaves stray continuation bytes to the converter")
{
    CHECK(CompleteUtf8Bytes("a\x80\x80\x80\x80") == 5);
}
