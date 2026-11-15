#pragma once

#include <cstddef>
#include <string>

#include <windows.h>

// Writes one crash report: <base>.dmp (a minidump) and <base>.txt (what crashed, in plain text).
// Used by CrashReport.cpp from inside the crash handler, so after Prepare() it allocates nothing
// on the heap and takes no locks the crashed thread may hold.
namespace Core::Platform::CrashReport::Writer
{

// The custom exception code abort(), pure virtual calls and invalid CRT parameters are raised
// with; its first parameter points to a static text naming which one it was.
inline constexpr DWORD kReportedFailureCode = 0xE0D5C001;

// The command line option of the dump helper: Main.exe --crash-dump <pid> <thread id>
// <EXCEPTION_POINTERS address in hex> "<dump path>".
inline constexpr wchar_t kDumpHelperOption[] = L"--crash-dump";

// Loads dbghelp.dll and remembers the folder the reports go to (created when missing).
// Returns false when neither the game folder nor %LOCALAPPDATA% can hold the folder.
bool Prepare(const std::wstring& directory);

// The folder Prepare() chose.
const wchar_t* Directory();

// The exe Write() starts as the dump helper (normally the game itself); null or empty: the dump is
// written inside the crashed process.
void SetDumpHelper(const wchar_t* exePath);

// Runs in the dump helper: writes the dump of process processId and returns the helper's exit code
// (0 when written). Ends both processes when dbghelp hangs.
int RunDumpHelper(DWORD processId, DWORD threadId, ULONG_PTR exceptionPointers, const wchar_t* dumpPath);

// Writes the pair for an exception of thread threadId. On success, outDumpPath and outTextPath
// (each pathCapacity wide characters) hold the two files' full paths. The text file comes first
// (it needs no heap), then textWritten (when not null) is set, then the dump (withDump, by the
// helper when one is set), and the
// text is rewritten with the dump's result. Returns false when not even the text could be written.
bool Write(const EXCEPTION_POINTERS* exception, DWORD threadId, wchar_t* outDumpPath, wchar_t* outTextPath,
           std::size_t pathCapacity, HANDLE textWritten, bool withDump);

// Deletes the oldest reports so at most keep pairs stay in the folder.
void PruneOldReports(std::size_t keep);

} // namespace Core::Platform::CrashReport::Writer
