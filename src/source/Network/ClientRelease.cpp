// Needs nothing of stdafx.h, so the unit test (tests/network/test_client_release.cpp) compiles it alone.
#include "Network/ClientRelease.h"

namespace Network::ClientRelease
{
namespace
{
// A 'D' stands for a digit; every other character has to be there as it is.
constexpr std::string_view ReleasePattern = "DDDD.DD.DD-DDDD";
constexpr char DigitSlot = 'D';
static_assert(ReleasePattern.size() == ReleaseLength);

// What may follow the number on its line (the LF ends the line itself).
constexpr std::string_view TrailingBlanks = " \t\r";

bool IsDigit(char character)
{
    return character >= '0' && character <= '9';
}

bool MatchesPattern(std::string_view text)
{
    if (text.size() != ReleasePattern.size())
        return false;

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const bool expectsDigit = ReleasePattern[i] == DigitSlot;
        const bool matches = expectsDigit ? IsDigit(text[i]) : text[i] == ReleasePattern[i];
        if (!matches)
            return false;
    }
    return true;
}

std::string_view FirstLine(std::string_view text)
{
    return text.substr(0, text.find('\n'));
}

std::string_view WithoutTrailingBlanks(std::string_view line)
{
    const std::size_t last = line.find_last_not_of(TrailingBlanks);
    if (last == std::string_view::npos)
        return {};
    return line.substr(0, last + 1);
}
} // namespace

std::optional<std::string> ParseReleaseText(std::string_view fileText)
{
    const std::string_view release = WithoutTrailingBlanks(FirstLine(fileText));
    if (!MatchesPattern(release))
        return std::nullopt;
    return std::string(release);
}
} // namespace Network::ClientRelease
