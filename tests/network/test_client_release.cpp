#include "Network/ClientRelease.h"

#include "doctest.h"

#include <string>
#include <string_view>

namespace
{
using Network::ClientRelease::ParseReleaseText;

constexpr char Release[] = "2026.11.22-0109";

bool Rejected(std::string_view fileText)
{
    return !ParseReleaseText(fileText).has_value();
}

std::string Parsed(std::string_view fileText)
{
    return ParseReleaseText(fileText).value_or("<none>");
}
} // namespace

TEST_CASE("Client release: a valid number alone is taken")
{
    CHECK(Parsed(Release) == Release);
}

TEST_CASE("Client release: the line end CRLF or LF is dropped")
{
    CHECK(Parsed("2026.11.22-0109\r\n") == Release);
    CHECK(Parsed("2026.11.22-0109\n") == Release);
    CHECK(Parsed("2026.11.22-0109\r") == Release);
}

TEST_CASE("Client release: only the first line counts")
{
    CHECK(Parsed("2026.11.22-0109\r\nanything else\r\n") == Release);
    CHECK(Rejected("\r\n2026.11.22-0109\r\n"));
}

TEST_CASE("Client release: spaces and tabs at the end are dropped")
{
    CHECK(Parsed("2026.11.22-0109   \r\n") == Release);
    CHECK(Parsed("2026.11.22-0109\t \r\n") == Release);
}

TEST_CASE("Client release: spaces before or inside the number are refused")
{
    CHECK(Rejected(" 2026.11.22-0109\r\n"));
    CHECK(Rejected("2026.11.22 -0109\r\n"));
}

TEST_CASE("Client release: a short or long line is refused")
{
    CHECK(Rejected("2026.11.22-010\r\n"));
    CHECK(Rejected("2026.11.22\r\n"));
    CHECK(Rejected("2026.11.22-01090\r\n"));
}

TEST_CASE("Client release: letters or other separators are refused")
{
    CHECK(Rejected("2026.11.2a-0109\r\n"));
    CHECK(Rejected("abcd.ef.gh-ijkl\r\n"));
    CHECK(Rejected("2026-11-22-0109\r\n"));
    CHECK(Rejected("2026.11.22.0109\r\n"));
}

TEST_CASE("Client release: an empty file or a blank line is refused")
{
    CHECK(Rejected(""));
    CHECK(Rejected("\r\n"));
    CHECK(Rejected("   \r\n"));
}
