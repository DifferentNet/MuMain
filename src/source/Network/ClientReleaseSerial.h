#pragma once

// The PC client's login packets carry its release number as their serial (Network/ClientRelease.h), so the
// server can turn away a client that is not the latest release.
namespace Network::ClientRelease
{
// Reads release.txt from the folder of Main.exe into the serial every login packet sends (the first login, the
// automatic re-login and the castle siege server change all send the same one), and writes to MuError.log which
// release was read. Without the file, or with no valid number in it, the serial stays the old fixed one. Call
// once at start-up, before the first connection. PC only: the browser build keeps the old serial.
void LoadIntoSerial();
} // namespace Network::ClientRelease
