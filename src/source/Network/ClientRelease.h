#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// The PC client's release number. The release script (scripts/publish-client.py in the server repository) writes
// it to release.txt next to Main.exe, one ASCII line "YYYY.MM.DD-HHMM"; the client sends it as the serial of its
// login packets (Network/ClientReleaseSerial.h), so the server can turn away a client that is not the latest one.
namespace Network::ClientRelease
{
// "2026.11.22-0109": with its terminator it fills the 16 bytes of the login packets' serial.
inline constexpr std::size_t ReleaseLength = 15;

// The release number in the text of release.txt: its first line without the spaces, tabs and CR at its end, when
// that is exactly "DDDD.DD.DD-DDDD" (D a digit). Nothing for any other text (empty, short, letters, ...).
std::optional<std::string> ParseReleaseText(std::string_view fileText);
} // namespace Network::ClientRelease
