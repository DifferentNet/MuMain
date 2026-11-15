#include "Core/Platform/win32/CrashReportWriter.h"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <vector>

#include <dbghelp.h>

#include "Core/Utilities/BuildInfo.h"

#if __has_include("MuGitCommit.h")
#include "MuGitCommit.h"
#endif

namespace Core::Platform::CrashReport::Writer
{
namespace
{

using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                          PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);

constexpr std::size_t kPathCapacity = MAX_PATH * 2;
constexpr std::size_t kTextCapacity = 16 * 1024;
constexpr int kMaxStackFrames = 48;
constexpr DWORD kCppExceptionCode = 0xE06D7363;  // what MSVC throws a C++ exception with
constexpr ULONG_PTR kCppExceptionMagic = 0x19930520;
constexpr DWORD kHeapCorruptionCode = 0xC0000374;      // STATUS_HEAP_CORRUPTION (ntstatus.h)
constexpr DWORD kStackBufferOverrunCode = 0xC0000409;  // STATUS_STACK_BUFFER_OVERRUN (ntstatus.h)
constexpr wchar_t kFolderName[] = L"CrashReports";
constexpr wchar_t kFilePrefix[] = L"crash-";
constexpr wchar_t kFallbackParent[] = L"MuSkynet";

// Small dumps (a few MB): every thread's stack and registers, the memory those point to, the
// module list. The game's big global arrays stay out, which keeps the file small enough to send.
constexpr MINIDUMP_TYPE kDumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
                                                               MiniDumpWithThreadInfo |
                                                               MiniDumpWithUnloadedModules);

MiniDumpWriteDumpFn s_miniDumpWriteDump = nullptr;
RtlGetVersionFn s_rtlGetVersion = nullptr;
wchar_t s_directory[kPathCapacity] = {};
char s_text[kTextCapacity] = {};
std::size_t s_textLength = 0;

// ---- building the text without the heap ----

void Append(const char* format, ...)
{
    if (s_textLength >= kTextCapacity - 1)
        return;
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(s_text + s_textLength, kTextCapacity - s_textLength, format, args);
    va_end(args);
    if (written > 0)
        s_textLength = (std::min)(s_textLength + static_cast<std::size_t>(written), kTextCapacity - 1);
}

void AppendWide(const wchar_t* text)
{
    if (s_textLength >= kTextCapacity - 1)
        return;
    const int room = static_cast<int>(kTextCapacity - 1 - s_textLength);
    const int written = WideCharToMultiByte(CP_UTF8, 0, text, -1, s_text + s_textLength, room, nullptr, nullptr);
    // The count includes the terminating zero WideCharToMultiByte wrote.
    if (written > 0)
        s_textLength += static_cast<std::size_t>(written - 1);
    s_text[s_textLength] = '\0';
}

// "Main.exe+0x1a2b3c" for a code address, "?" outside every module.
void AppendAddress(DWORD64 address)
{
    HMODULE module = nullptr;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    wchar_t path[kPathCapacity] = {};
    if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(address), &module) ||
        GetModuleFileNameW(module, path, static_cast<DWORD>(kPathCapacity)) == 0)
    {
        Append("0x%016llx ?", static_cast<unsigned long long>(address));
        return;
    }
    const wchar_t* name = std::wcsrchr(path, L'\\');
    const DWORD64 offset = address - reinterpret_cast<DWORD64>(module);
    Append("0x%016llx ", static_cast<unsigned long long>(address));
    AppendWide(name != nullptr ? name + 1 : path);
    Append("+0x%llx", static_cast<unsigned long long>(offset));
}

// ---- what the exception was ----

const char* ExceptionName(DWORD code)
{
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION: return "access violation";
        case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
        case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
        case EXCEPTION_IN_PAGE_ERROR: return "in-page error (a file could not be read)";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
        case kHeapCorruptionCode: return "heap corruption";
        case kStackBufferOverrunCode: return "stack buffer overrun";
        case kCppExceptionCode: return "uncaught C++ exception";
        case kReportedFailureCode: return "fatal runtime error";
        default: return "unknown exception";
    }
}

// The decorated type name of a thrown C++ object (".?AVruntime_error@std@@"), read from the
// throw information MSVC passes with the exception and copied into `out` inside the guard (the
// throwing module may be gone). Separate function: __try can't share a function with objects
// that need unwinding. MSVC only: other compilers have neither __try nor this layout.
bool CopyCppExceptionTypeName(const EXCEPTION_RECORD& record, char* out, std::size_t capacity)
{
#if defined(_M_X64) && defined(_MSC_VER)
    if (record.NumberParameters < 4 || record.ExceptionInformation[0] != kCppExceptionMagic || capacity == 0)
        return false;
    __try
    {
        const auto imageBase = static_cast<std::uintptr_t>(record.ExceptionInformation[3]);
        const auto* throwInfo = reinterpret_cast<const std::int32_t*>(record.ExceptionInformation[2]);
        // ThrowInfo: attributes, pmfnUnwind, pForwardCompat, pCatchableTypeArray (image-relative).
        const auto* catchableTypes = reinterpret_cast<const std::int32_t*>(imageBase + throwInfo[3]);
        // CatchableTypeArray: count, then the types; CatchableType: properties, pType, ...
        const auto* firstType = reinterpret_cast<const std::int32_t*>(imageBase + catchableTypes[1]);
        // TypeDescriptor: vftable pointer, spare pointer, then the name.
        const char* name = reinterpret_cast<const char*>(imageBase + firstType[1]) + 2 * sizeof(void*);
        std::size_t length = 0;
        while (length + 1 < capacity && name[length] != '\0')
        {
            out[length] = name[length];
            ++length;
        }
        out[length] = '\0';
        return length > 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out[0] = '\0';
        return false;
    }
#else
    (void)record;
    (void)out;
    (void)capacity;
    return false;
#endif
}

void AppendException(const EXCEPTION_RECORD& record, DWORD threadId)
{
    Append("Exception: 0x%08lx, %s\r\n", record.ExceptionCode, ExceptionName(record.ExceptionCode));
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
    {
        const ULONG_PTR kind = record.ExceptionInformation[0];
        const char* verb = kind == 0 ? "reading" : kind == 1 ? "writing" : "executing";
        Append("  while %s address 0x%016llx\r\n", verb,
               static_cast<unsigned long long>(record.ExceptionInformation[1]));
    }
    if (record.ExceptionCode == kReportedFailureCode && record.NumberParameters >= 1 &&
        record.ExceptionInformation[0] != 0)
        Append("  %s\r\n", reinterpret_cast<const char*>(record.ExceptionInformation[0]));
    if (record.ExceptionCode == kCppExceptionCode)
    {
        static char typeName[256];
        if (CopyCppExceptionTypeName(record, typeName, sizeof(typeName)))
            Append("  type %s\r\n", typeName);
    }
    Append("Address: ");
    AppendAddress(reinterpret_cast<DWORD64>(record.ExceptionAddress));
    Append("\r\nThread: %lu\r\n", threadId);
}

// ---- the call stack, unwound from the crash context without symbols ----

#if defined(_M_X64) && defined(_MSC_VER)
// One unwind step; false at the end of the stack or when the stack can't be read.
bool UnwindOneFrame(CONTEXT& context)
{
    __try
    {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        if (function == nullptr)
        {
            // A leaf function: the return address is on top of the stack.
            context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
            context.Rsp += sizeof(DWORD64);
        }
        else
        {
            void* handlerData = nullptr;
            DWORD64 establisherFrame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData,
                             &establisherFrame, nullptr);
        }
        return context.Rip != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
#endif

void AppendCallStack(const CONTEXT* crashContext)
{
#if defined(_M_X64) && defined(_MSC_VER)
    if (crashContext == nullptr)
        return;
    Append("\r\nCall stack (module+offset; Main.map or Main.pdb of this build names the functions):\r\n");
    CONTEXT context = *crashContext;
    for (int frame = 0; frame < kMaxStackFrames; ++frame)
    {
        Append("  %2d ", frame);
        AppendAddress(context.Rip);
        Append("\r\n");
        if (!UnwindOneFrame(context))
            break;
    }
#else
    (void)crashContext;
#endif
}

// ---- which build and which Windows ----

DWORD ExeLinkTimestamp()
{
    const auto* base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp;
}

void AppendBuild()
{
    Append("\r\nBuild: %s %s %s, compiled %s %s\r\n", Core::Build::kConfiguration, Core::Build::kCompiler,
           Core::Build::kArchitecture, Core::Build::kDate, Core::Build::kTime);
#ifdef MU_GIT_COMMIT
    Append("Commit: %s%s\r\n", MU_GIT_COMMIT, MU_GIT_COMMIT_CHANGED ? " (with local changes)" : "");
#endif
    // Matches the Main.pdb of this exact Main.exe (the debugger checks the same stamp).
    Append("Main.exe link stamp: 0x%08lx\r\n", ExeLinkTimestamp());
    wchar_t exePath[kPathCapacity] = {};
    if (GetModuleFileNameW(nullptr, exePath, static_cast<DWORD>(kPathCapacity)) != 0)
    {
        Append("Main.exe: ");
        AppendWide(exePath);
        Append("\r\n");
    }
    OSVERSIONINFOW version = {};
    version.dwOSVersionInfoSize = sizeof(version);
    if (s_rtlGetVersion != nullptr && s_rtlGetVersion(&version) == 0)
        Append("Windows: %lu.%lu build %lu\r\n", version.dwMajorVersion, version.dwMinorVersion,
               version.dwBuildNumber);
}

// ---- files ----

bool FormatPath(wchar_t* out, std::size_t capacity, const wchar_t* baseName, const wchar_t* extension)
{
    const int written = std::swprintf(out, capacity, L"%ls\\%ls%ls", s_directory, baseName, extension);
    return written > 0 && static_cast<std::size_t>(written) < capacity;
}

bool WriteDump(const wchar_t* path, const EXCEPTION_POINTERS* exception, DWORD threadId)
{
    if (s_miniDumpWriteDump == nullptr)
        return false;
    // FILE_SHARE_DELETE: a dump that hangs is deleted by the crashed thread while this handle is open.
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    MINIDUMP_EXCEPTION_INFORMATION info = {};
    info.ThreadId = threadId;
    info.ExceptionPointers = const_cast<EXCEPTION_POINTERS*>(exception);
    info.ClientPointers = FALSE;
    const BOOL written = s_miniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, kDumpType,
                                             exception != nullptr ? &info : nullptr, nullptr, nullptr);
    CloseHandle(file);
    if (!written)
        DeleteFileW(path);
    return written != FALSE;
}

bool WriteText(const wchar_t* path)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, s_text, static_cast<DWORD>(s_textLength), &written, nullptr);
    CloseHandle(file);
    return ok && written == s_textLength;
}

bool CanWriteInto(const wchar_t* directory)
{
    if (!CreateDirectoryW(directory, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    wchar_t probe[kPathCapacity] = {};
    const int written = std::swprintf(probe, kPathCapacity, L"%ls\\.write-test", directory);
    if (written <= 0 || static_cast<std::size_t>(written) >= kPathCapacity)
        return false;
    HANDLE file = CreateFileW(probe, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(file);
    return true;
}

// %LOCALAPPDATA%\MuSkynet\CrashReports, for a game folder the player can't write into.
bool UseFallbackDirectory()
{
    wchar_t localAppData[kPathCapacity] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(kPathCapacity));
    if (length == 0 || length >= kPathCapacity)
        return false;
    wchar_t parent[kPathCapacity] = {};
    std::swprintf(parent, kPathCapacity, L"%ls\\%ls", localAppData, kFallbackParent);
    CreateDirectoryW(parent, nullptr);
    std::swprintf(s_directory, kPathCapacity, L"%ls\\%ls", parent, kFolderName);
    return CanWriteInto(s_directory);
}

} // namespace

bool Prepare(const std::wstring& directory)
{
    if (HMODULE dbghelp = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
        s_miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
        s_rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));

    if (directory.size() < kPathCapacity)
    {
        wcscpy_s(s_directory, directory.c_str());
        if (CanWriteInto(s_directory))
            return true;
    }
    return UseFallbackDirectory();
}

const wchar_t* Directory()
{
    return s_directory;
}

bool Write(const EXCEPTION_POINTERS* exception, DWORD threadId, wchar_t* outDumpPath, wchar_t* outTextPath,
           std::size_t pathCapacity, HANDLE textWritten, bool withDump)
{
    if (s_directory[0] == L'\0' || outDumpPath == nullptr || outTextPath == nullptr)
        return false;
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    wchar_t baseName[64] = {};
    std::swprintf(baseName, 64, L"%ls%04u%02u%02u-%02u%02u%02u", kFilePrefix, now.wYear, now.wMonth, now.wDay,
                  now.wHour, now.wMinute, now.wSecond);
    if (!FormatPath(outDumpPath, pathCapacity, baseName, L".dmp") ||
        !FormatPath(outTextPath, pathCapacity, baseName, L".txt"))
        return false;

    s_textLength = 0;
    s_text[0] = '\0';
    Append("MU Skynet crash report\r\nTime: %04u-%02u-%02u %02u:%02u:%02u (local)\r\n", now.wYear, now.wMonth,
           now.wDay, now.wHour, now.wMinute, now.wSecond);
    if (exception != nullptr && exception->ExceptionRecord != nullptr)
        AppendException(*exception->ExceptionRecord, threadId);
    AppendBuild();
    AppendCallStack(exception != nullptr ? exception->ContextRecord : nullptr);

    // The text first: it needs no heap, the dump does and may hang or fail on a damaged one.
    const std::size_t lengthBeforeDump = s_textLength;
    Append(withDump ? "\r\nDump: not written (the game ended while writing it)\r\n"
                    : "\r\nDump: not written (the crash left the process unable to write one)\r\n");
    const bool textOk = WriteText(outTextPath);
    if (textOk && textWritten != nullptr)
        SetEvent(textWritten);
    if (!withDump)
    {
        outDumpPath[0] = L'\0';
        return textOk;
    }

    const bool dumpWritten = WriteDump(outDumpPath, exception, threadId);
    s_textLength = lengthBeforeDump;
    s_text[s_textLength] = '\0';
    Append("\r\nDump: %s\r\n", dumpWritten ? "written" : "could not be written");
    if (!dumpWritten)
        outDumpPath[0] = L'\0';
    return WriteText(outTextPath) || textOk;
}

void PruneOldReports(std::size_t keep)
{
    if (s_directory[0] == L'\0')
        return;
    wchar_t pattern[kPathCapacity] = {};
    std::swprintf(pattern, kPathCapacity, L"%ls\\%ls*", s_directory, kFilePrefix);
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
        return;
    std::vector<std::wstring> names;
    do
    {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            names.emplace_back(found.cFileName);
    } while (FindNextFileW(search, &found));
    FindClose(search);

    // crash-YYYYMMDD-HHMMSS.*: sorting the names sorts by time; a pair shares the name before the dot.
    std::vector<std::wstring> reports;
    for (const std::wstring& name : names)
        reports.push_back(name.substr(0, name.find(L'.')));
    std::sort(reports.begin(), reports.end());
    reports.erase(std::unique(reports.begin(), reports.end()), reports.end());
    if (reports.size() <= keep)
        return;
    reports.resize(reports.size() - keep);  // the oldest ones

    for (const std::wstring& name : names)
    {
        const std::wstring report = name.substr(0, name.find(L'.'));
        if (std::binary_search(reports.begin(), reports.end(), report))
            DeleteFileW((std::wstring(s_directory) + L"\\" + name).c_str());
    }
}

} // namespace Core::Platform::CrashReport::Writer
