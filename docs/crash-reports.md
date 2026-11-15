# Crash reports

When the Windows client crashes, it leaves a report the player can send us, and closes.

## What the player sees

The game closes at once (it does not stay connected to the server). A second copy, `Main.exe --crash-notice <txt>
<dmp>`, shows a dialog in Russian and English: the game closed because of an error, the folder with the report, and a
request to send both files to an admin in a private Discord message, not a public channel, since the dump can hold the
player's login details. **Yes** opens the folder in Explorer with the report selected.

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
ends without running any handler; those still appear in Windows' own error reporting. `std::terminate()` on a thread
other than the main one ends in `abort()`, so its report says "abort() was called".

The text is written by a thread started with the game, which sleeps until a crash. The dump is written by a second
copy, `Main.exe --crash-dump <pid> <thread> <pointers> <dmp>`, which reads the crashed game from outside, so it takes
no lock the game holds; the `.txt` says "Dump: written by a second copy of the game". Only when that copy can't be
started is the dump written inside the game ("written inside the game"). The second copy gives up after 15 seconds
and then ends the crashed game too. When the writer can't finish (the crashed thread held the heap lock, inside
`malloc` on a damaged heap), the game closes without the dialog and with the `.txt` only ("Dump: not written"):
after 5 seconds when not even the text got written (the crashed thread then writes it itself), after 30 seconds when
the dump hung (its half-written `.dmp` is removed). A `CrashReports` folder deleted while the game ran is made again.

While the dialog is open, the launcher closes it (the dialog copy holds the event `Local\MuSkynet-CrashNotice-<pid>`)
instead of counting it as a running game.

## Reading a report

The `.txt` link stamp matches the release's symbols, which `scripts/publish-client.py` of the server repository keeps
for every publish (server `docs/release.md`, "Crash report symbols"). Open the `.dmp` in Visual Studio or WinDbg with
that `Main.pdb`; without it, look up the call stack offsets in `Main.map`.

## Tests and automated runs

`MU_CRASH_REPORT_NO_DIALOG=1` writes the report without showing the dialog; a client started with `MU_CONTROL_SOCKET`
(the in-game tests) shows none either. With `MU_CRASH_REPORT_NOTICE_LOG=<file>` the `--crash-notice` copy writes the
dialog's text (UTF-16) to that file instead of showing it. The `crash_report` test
(`tests/platform/test_crash_report.py`) crashes `crash_report_probe` each of the ways above and checks the files, then
checks that each dump was written by the second copy and holds the exception, also for an exception escaping a
thread and for a `CrashReports` folder deleted before the crash, then crashes it once with the dialog wanted and
checks that the second copy names the report.
