#pragma once

// Crash reports (Windows): when the game crashes, it writes a minidump and a short text report into
// the CrashReports folder next to Main.exe, closes, and a second copy started with --crash-notice
// tells the player where they are. The dump
// opens in Visual Studio or WinDbg with the release's Main.pdb (kept by scripts/publish-client.py);
// the text names the exception, the module and offset of the faulting code and the build.
namespace Core::Platform::CrashReport
{

// Call once at start-up, as early as possible. Catches unhandled exceptions (access violations,
// stack overflows, uncaught C++ exceptions) and abort(), pure virtual calls, invalid CRT
// parameters. Also removes all but the newest reports. Does nothing off Windows.
void Install();

// When the game was started as the crash dialog of a crashed copy (Main.exe --crash-notice <txt>
// <dmp>), shows that dialog and returns true: the caller exits without starting the game.
// When started as the dump writer of a crashed copy (Main.exe --crash-dump <pid> <thread>
// <pointers> <dmp>), writes the dump and exits the process without returning.
// Returns false otherwise.
bool RunNoticeIfAsked();

} // namespace Core::Platform::CrashReport
