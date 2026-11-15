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

// Loads dbghelp.dll and remembers the folder the reports go to (created when missing).
// Returns false when neither the game folder nor %LOCALAPPDATA% can hold the folder.
bool Prepare(const std::wstring& directory);

// The folder Prepare() chose.
const wchar_t* Directory();

// Writes the pair for an exception of thread threadId. On success, outDumpPath and outTextPath
// (each pathCapacity wide characters) hold the two files' full paths. The text file comes first
// (it needs no heap), then textWritten (when not null) is set, then the dump (withDump), and the
// text is rewritten with the dump's result. Returns false when not even the text could be written.
bool Write(const EXCEPTION_POINTERS* exception, DWORD threadId, wchar_t* outDumpPath, wchar_t* outTextPath,
           std::size_t pathCapacity, HANDLE textWritten, bool withDump);

// Deletes the oldest reports so at most keep pairs stay in the folder.
void PruneOldReports(std::size_t keep);

} // namespace Core::Platform::CrashReport::Writer
