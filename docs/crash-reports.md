# Crash reports

When the Windows client crashes, it leaves a report the player can send us, and closes.

## What the player sees

A dialog in Russian and English says the game closed because of an error, names the folder with the report and asks
the player to send both files on Discord. **Yes** opens the folder in Explorer with the report selected.

## Where the report is

`CrashReports\` next to `Main.exe`. When the player can't write there (the game installed under Program Files), it
goes to `%LOCALAPPDATA%\MuSkynet\CrashReports\` instead. Each crash writes a pair with the same name:

- `crash-YYYYMMDD-HHMMSS.txt`: what crashed (exception, address, for an uncaught C++ exception its type), the
  module and offset of the faulting code, the build (configuration, commit, `Main.exe` link stamp), the Windows
  version and the call stack as module + offset.
- `crash-YYYYMMDD-HHMMSS.dmp`: a small minidump (a few MB): every thread's stack and registers and the memory they
  point to.

The newest 10 reports stay; older ones are removed at the next start.

## What is caught

Access violations, stack overflows, uncaught C++ exceptions, heap corruption, `abort()`, `std::terminate()`, pure
virtual calls and invalid parameters to C runtime functions, on any thread. Not caught: the process being killed, a
GPU driver reset that only hangs the game, and fast-fail checks (`/GS` buffer overruns, `__fastfail`), which Windows
ends without running any handler; those still appear in Windows' own error reporting.

The report is written by a thread started with the game, which sleeps until a crash. When it can't finish (the
crashed thread held the heap lock, inside `malloc` on a damaged heap), the game closes after 5 seconds with the
`.txt` only ("Dump: not written") and without the dialog, which would wait for the same lock.

## Reading a report

The `.txt` link stamp matches the release's symbols, which `scripts/publish-client.py` of the server repository keeps
for every publish (server `docs/release.md`, "Crash report symbols"). Open the `.dmp` in Visual Studio or WinDbg with
that `Main.pdb`; without it, look up the call stack offsets in `Main.map`.

## Tests and automated runs

`MU_CRASH_REPORT_NO_DIALOG=1` writes the report without showing the dialog. The `crash_report` test
(`tests/platform/test_crash_report.py`) crashes `crash_report_probe` each of the ways above and checks the files.
