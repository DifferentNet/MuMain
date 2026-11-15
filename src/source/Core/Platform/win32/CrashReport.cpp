#include "Core/Platform/CrashReport.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <exception>
#include <string>

#include <windows.h>

#include <shellapi.h>

#include "Core/Platform/win32/CrashReportWriter.h"

namespace Core::Platform::CrashReport
{
namespace
{

constexpr std::size_t kPathCapacity = MAX_PATH * 2;
constexpr std::size_t kNoticeCapacity = 4096;
constexpr wchar_t kNoticeOption[] = L"--crash-notice";
// The notice copy holds this event (with its own process id) while it runs, so the launcher tells it
// from a running game (launcher/Updater.cs on the server).
constexpr wchar_t kNoticeEventPrefix[] = L"Local\\MuSkynet-CrashNotice-";
constexpr std::size_t kReportsKept = 10;
constexpr wchar_t kFolderName[] = L"CrashReports";
// Set to 1 by tests and automated runs: write the report, show no dialog. A client driven over the
// control socket (the in-game tests) shows none either.
constexpr wchar_t kNoDialogVariable[] = L"MU_CRASH_REPORT_NO_DIALOG";
constexpr wchar_t kControlSocketVariable[] = L"MU_CONTROL_SOCKET";
// Tests: the notice process writes its text to this file instead of showing the dialog.
constexpr wchar_t kNoticeLogVariable[] = L"MU_CRASH_REPORT_NOTICE_LOG";
// Room the crash handler keeps on a thread whose stack overflowed.
constexpr ULONG kStackGuaranteeBytes = 64 * 1024;
constexpr SIZE_T kWriterStackBytes = 512 * 1024;
// The dump takes seconds. Longer means the writer waits for a lock the crashed thread holds (the
// heap's, inside malloc on a damaged heap): then the game closes with the text report only.
constexpr DWORD kTextTimeoutMs = 5 * 1000;
// Longer than the writer's wait for the dump helper (20 s from its start), so a helper that starts
// slowly still finishes a dump this thread would otherwise throw away.
constexpr DWORD kWriteTimeoutMs = 30 * 1000;

struct CrashContext
{
    EXCEPTION_POINTERS* exception = nullptr;
    DWORD threadId = 0;
    HANDLE wake = nullptr;         // set by the crashed thread: write the report
    HANDLE textWritten = nullptr;  // set once the .txt is on disk
    HANDLE written = nullptr;      // set once both files are on disk and the dialog is started
    wchar_t dumpPath[kPathCapacity] = {};
    wchar_t textPath[kPathCapacity] = {};
    bool ok = false;
    volatile LONG writeDone = 0;  // set by the writer once Write() returned: the dump is finished
};

constexpr DWORD kHeapCorruptionCode = 0xC0000374;  // STATUS_HEAP_CORRUPTION (ntstatus.h)

LONG s_handling = 0;
// ReportFailure's text: the record points at it while the report is written.
char s_failureText[kFailureTextCapacity] = {};
DWORD s_handlingThreadId = 0;
DWORD s_writerThreadId = 0;
HANDLE s_writer = nullptr;
bool s_showDialog = true;
CrashContext s_crash;
wchar_t s_notice[kNoticeCapacity] = {};

const wchar_t* FileName(const wchar_t* path)
{
    const wchar_t* slash = std::wcsrchr(path, L'\\');
    return slash != nullptr ? slash + 1 : path;
}

// The dialog is shown by a second copy of the game (RunNoticeIfAsked), so the crashed one closes at
// once: it doesn't stay connected, and its other threads don't run on behind the dialog.
wchar_t s_exePath[kPathCapacity] = {};
wchar_t s_noticeCommandLine[kPathCapacity * 3 + 64] = {};

void StartNotice(const CrashContext& crash)
{
    if (!s_showDialog || s_exePath[0] == L'\0')
        return;
    std::swprintf(s_noticeCommandLine, sizeof(s_noticeCommandLine) / sizeof(wchar_t), L"\"%ls\" %ls \"%ls\" \"%ls\"",
                  s_exePath, kNoticeOption, crash.ok ? crash.textPath : L"", crash.ok ? crash.dumpPath : L"");
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(s_exePath, s_noticeCommandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                       &process))
    {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

// Started by Install() and asleep until a crash: a thread started during the crash could hang in
// its own start-up (it takes the loader lock, then the heap's, which the crashed thread may hold).
DWORD WINAPI WriterThreadMain(LPVOID parameter)
{
    auto* crash = static_cast<CrashContext*>(parameter);
    // The C runtime allocates a thread's data on its first use; do it now, while the heap works.
    wchar_t warmUp[16] = {};
    std::swprintf(warmUp, 16, L"%d", static_cast<int>(errno));
    if (WaitForSingleObject(crash->wake, INFINITE) != WAIT_OBJECT_0)
        return 0;
    crash->ok = Writer::Write(crash->exception, crash->threadId, crash->dumpPath, crash->textPath, kPathCapacity,
                              crash->textWritten, true);
    InterlockedExchange(&crash->writeDone, 1);
    StartNotice(*crash);
    SetEvent(crash->written);
    return 0;
}

bool StartWriter()
{
    s_crash.wake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_crash.textWritten = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_crash.written = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (s_crash.wake == nullptr || s_crash.textWritten == nullptr || s_crash.written == nullptr)
        return false;
    s_writer = CreateThread(nullptr, kWriterStackBytes, WriterThreadMain, &s_crash, STACK_SIZE_PARAM_IS_A_RESERVATION,
                            &s_writerThreadId);
    return s_writer != nullptr;
}

// Runs on the crashed thread. The report is written on the writer thread: the crashed one may have
// no stack left (a stack overflow) and a dump is cleaner when written from outside.
void ReportAndExit(EXCEPTION_POINTERS* exception)
{
    s_crash.exception = exception;
    s_crash.threadId = GetCurrentThreadId();
    if (s_writer == nullptr)
    {
        s_crash.ok = Writer::Write(exception, s_crash.threadId, s_crash.dumpPath, s_crash.textPath, kPathCapacity,
                                   nullptr, true);
        StartNotice(s_crash);
    }
    else
    {
        SetEvent(s_crash.wake);
        // The writer may also finish without the text (no folder, a full disk): then don't wait on.
        const HANDLE firstSign[] = {s_crash.textWritten, s_crash.written};
        const DWORD first = WaitForMultipleObjects(2, firstSign, FALSE, kTextTimeoutMs);
        const bool writerDone = first == WAIT_OBJECT_0 + 1 ||
                                (first == WAIT_OBJECT_0 &&
                                 WaitForSingleObject(s_crash.written, kWriteTimeoutMs) == WAIT_OBJECT_0);
        if (!writerDone)
        {
            // The writer waits for a lock this thread holds. Keep its text report, or write one from
            // here; no dump (a half-written one is removed) and no dialog, which would wait for the
            // same locks.
            SuspendThread(s_writer);
            if (WaitForSingleObject(s_crash.textWritten, 0) == WAIT_OBJECT_0)
            {
                // The dump hung: remove it (gone once the writer's handle closes with the process). A finished
                // one stays; then only the dialog failed to start.
                if (InterlockedCompareExchange(&s_crash.writeDone, 0, 0) == 0)
                    DeleteFileW(s_crash.dumpPath);
            }
            else
                Writer::Write(exception, s_crash.threadId, s_crash.dumpPath, s_crash.textPath, kPathCapacity, nullptr,
                              false);
        }
    }
    const UINT exitCode = exception != nullptr && exception->ExceptionRecord != nullptr
                              ? exception->ExceptionRecord->ExceptionCode
                              : EXIT_FAILURE;
    TerminateProcess(GetCurrentProcess(), exitCode);
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exception)
{
    if (InterlockedExchange(&s_handling, 1) != 0)
    {
        // The writer itself crashed: let the crashed thread go on to close the game.
        if (GetCurrentThreadId() == s_writerThreadId)
        {
            SetEvent(s_crash.written);
            ExitThread(EXIT_FAILURE);
        }
        // Writing from the crashed thread crashed again: nothing more to save, close the game.
        if (GetCurrentThreadId() == s_handlingThreadId)
            TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
        // Another thread crashed at the same time: the first report closes the game.
        Sleep(INFINITE);
    }
    s_handlingThreadId = GetCurrentThreadId();
    ReportAndExit(exception);
    return EXCEPTION_EXECUTE_HANDLER;
}

// Windows ends the process on heap corruption without calling the unhandled exception filter;
// the exception passes the vectored handlers first, so report it from there.
LONG WINAPI OnVectoredException(EXCEPTION_POINTERS* exception)
{
    if (exception != nullptr && exception->ExceptionRecord != nullptr &&
        exception->ExceptionRecord->ExceptionCode == kHeapCorruptionCode)
        OnUnhandledException(exception);
    return EXCEPTION_CONTINUE_SEARCH;
}

// abort(), pure virtual calls, invalid CRT parameters and std::terminate() don't raise an
// exception by themselves, and the C runtime may call their handlers inside its own __try/__except
// (terminate() does, abort() included), which would swallow a raised one and end the game silently.
// So they report from here, with this thread's context as the crash. On the stack, not static: two
// threads may fail at once (the second waits in OnUnhandledException).
[[noreturn]] void ReportFromHere(const char* what)
{
    alignas(16) CONTEXT context = {};
    EXCEPTION_RECORD record = {};
    EXCEPTION_POINTERS pointers = {};
    RtlCaptureContext(&context);
    record.ExceptionCode = Writer::kReportedFailureCode;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#if defined(_M_X64)
    record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#elif defined(_M_IX86)
    record.ExceptionAddress = reinterpret_cast<void*>(context.Eip);
#endif
    record.NumberParameters = 1;
    record.ExceptionInformation[0] = reinterpret_cast<ULONG_PTR>(what);
    pointers.ExceptionRecord = &record;
    pointers.ContextRecord = &context;
    OnUnhandledException(&pointers);
    TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
    std::abort();
}

// Also the end of std::terminate() on threads other than the one Install() ran on: the C runtime
// keeps the terminate handler per thread, and the default one calls abort().
void OnAbortSignal(int)
{
    ReportFromHere("abort() was called (std::terminate() on another thread ends here too)");
}

void OnPureCall()
{
    ReportFromHere("pure virtual function call");
}

void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
    ReportFromHere("invalid parameter passed to a C runtime function");
}

void OnTerminate()
{
    ReportFromHere("std::terminate() was called (an exception left a noexcept function or a thread)");
}

std::wstring DefaultDirectory()
{
    wchar_t exePath[kPathCapacity] = {};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, static_cast<DWORD>(kPathCapacity));
    if (length == 0 || length >= kPathCapacity)
        return kFolderName;
    std::wstring directory(exePath, length);
    directory.resize(directory.find_last_of(L'\\') + 1);
    return directory + kFolderName;
}

bool DialogWanted()
{
    wchar_t value[8] = {};
    const DWORD length = GetEnvironmentVariableW(kNoDialogVariable, value, 8);
    if (length == 1 && value[0] == L'1')
        return false;
    return GetEnvironmentVariableW(kControlSocketVariable, value, 8) == 0;
}

// Whether the start of the report's text (ASCII, a few KB) contains `needle`.
bool ReportMentions(const wchar_t* textPath, const char* needle)
{
    HANDLE file = CreateFileW(textPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    char buffer[4096] = {};
    DWORD read = 0;
    const BOOL ok = ReadFile(file, buffer, sizeof(buffer) - 1, &read, nullptr);
    CloseHandle(file);
    return ok && read > 0 && std::strstr(buffer, needle) != nullptr;
}

void ComposeNotice(const wchar_t* textPath, const wchar_t* dumpPath)
{
    if (textPath[0] == L'\0')
    {
        std::swprintf(s_notice, kNoticeCapacity,
                      L"MU Skynet закрылась из-за ошибки. Отчёт записать не удалось.\n\n"
                      L"MU Skynet closed because of an error. The crash report could not be written.");
        return;
    }
    const std::wstring text(textPath);
    const std::wstring folder = text.substr(0, text.find_last_of(L'\\'));
    const wchar_t* gpuHint = ReportMentions(textPath, kGpuLostMarker)
                                 ? L"Видеокарта перестала отвечать. Обновите драйвер видеокарты.\n"
                                   L"The graphics card stopped responding. Please update its driver.\n\n"
                                 : L"";
    std::swprintf(s_notice, kNoticeCapacity,
                  L"MU Skynet закрылась из-за ошибки.\n\n%ls"
                  L"Отчёт сохранён в папке:\n%ls\n\n"
                  L"Пришлите оба файла (%ls и %ls) администратору в личные сообщения Discord, не в общий канал: "
                  L"в них могут быть данные вашего входа. По ним мы найдём и исправим причину.\n\n"
                  L"MU Skynet closed because of an error. The report is in the folder above; please send both "
                  L"files to an admin in a private Discord message, not a public channel (they may hold your "
                  L"login details), so we can fix it.\n\n"
                  L"Открыть папку? / Open the folder?",
                  gpuHint, folder.c_str(), FileName(textPath), dumpPath[0] != L'\0' ? FileName(dumpPath) : L"-");
}

void OpenFolderAt(const wchar_t* path)
{
    std::wstring commandLine = L"explorer.exe /select,\"" + std::wstring(path) + L"\"";
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
    {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

void WriteNoticeLog(const wchar_t* logPath)
{
    HANDLE file = CreateFileW(logPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(file, s_notice, static_cast<DWORD>(std::wcslen(s_notice) * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(file);
}

} // namespace

void ReportFailure(const char* what)
{
    strncpy_s(s_failureText, what != nullptr ? what : "unknown failure", _TRUNCATE);
    ReportFromHere(s_failureText);
}

bool RunNoticeIfAsked()
{
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr)
        return false;
    if (count == 6 && std::wcscmp(arguments[1], Writer::kDumpHelperOption) == 0)
    {
        const auto processId = static_cast<DWORD>(std::wcstoul(arguments[2], nullptr, 10));
        const auto threadId = static_cast<DWORD>(std::wcstoul(arguments[3], nullptr, 10));
        const auto pointers = static_cast<ULONG_PTR>(std::wcstoull(arguments[4], nullptr, 16));
        ExitProcess(static_cast<UINT>(Writer::RunDumpHelper(processId, threadId, pointers, arguments[5])));
    }
    const bool asked = count == 4 && std::wcscmp(arguments[1], kNoticeOption) == 0;
    if (asked)
    {
        wchar_t eventName[64] = {};
        std::swprintf(eventName, 64, L"%ls%lu", kNoticeEventPrefix, GetCurrentProcessId());
        HANDLE marker = CreateEventW(nullptr, TRUE, FALSE, eventName);  // lives until this copy exits
        (void)marker;
        ComposeNotice(arguments[2], arguments[3]);
        wchar_t logPath[kPathCapacity] = {};
        const DWORD logLength = GetEnvironmentVariableW(kNoticeLogVariable, logPath, static_cast<DWORD>(kPathCapacity));
        if (logLength > 0 && logLength < kPathCapacity)
        {
            WriteNoticeLog(logPath);
        }
        else
        {
            const UINT style =
                (arguments[2][0] != L'\0' ? MB_YESNO : MB_OK) | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND;
            if (MessageBoxW(nullptr, s_notice, L"MU Skynet", style) == IDYES)
                OpenFolderAt(arguments[2]);
        }
    }
    LocalFree(arguments);
    return asked;
}

void Install()
{
    s_showDialog = DialogWanted();
    const DWORD exeLength = GetModuleFileNameW(nullptr, s_exePath, static_cast<DWORD>(kPathCapacity));
    if (exeLength == 0 || exeLength >= kPathCapacity)
        s_exePath[0] = L'\0';
    Writer::SetDumpHelper(s_exePath);
    if (!Writer::Prepare(DefaultDirectory()))
        return;
    Writer::PruneOldReports(kReportsKept);
    StartWriter();  // without it the crashed thread writes the report itself

    ULONG guarantee = kStackGuaranteeBytes;
    SetThreadStackGuarantee(&guarantee);
    SetUnhandledExceptionFilter(OnUnhandledException);
    AddVectoredExceptionHandler(1, OnVectoredException);

    // The C runtime's own failure paths: no "abort() has been called" box, no silent fast-fail.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::signal(SIGABRT, OnAbortSignal);
    _set_purecall_handler(OnPureCall);
    _set_invalid_parameter_handler(OnInvalidParameter);
    std::set_terminate(OnTerminate);
}

} // namespace Core::Platform::CrashReport
