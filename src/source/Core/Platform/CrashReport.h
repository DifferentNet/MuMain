#pragma once

// Crash reports (Windows): when the game crashes, it writes a minidump and a short text report into
// the CrashReports folder next to Main.exe, tells the player where they are and closes. The dump
// opens in Visual Studio or WinDbg with the release's Main.pdb (kept by scripts/publish-client.py);
// the text names the exception, the module and offset of the faulting code and the build.
namespace Core::Platform::CrashReport
{

// Call once at start-up, as early as possible. Catches unhandled exceptions (access violations,
// stack overflows, uncaught C++ exceptions) and abort(), pure virtual calls, invalid CRT
// parameters. Also removes all but the newest reports. Does nothing off Windows.
void Install();

} // namespace Core::Platform::CrashReport
