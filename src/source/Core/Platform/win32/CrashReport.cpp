#include "Core/Platform/CrashReport.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <string>

#include <windows.h>

#include "Core/Platform/win32/CrashReportWriter.h"

namespace Core::Platform::CrashReport
{
namespace
{

constexpr std::size_t kPathCapacity = MAX_PATH * 2;
constexpr std::size_t kNoticeCapacity = 4096;
constexpr std::size_t kReportsKept = 10;
constexpr wchar_t kFolderName[] = L"CrashReports";
// Set to 1 by tests and automated runs: write the report, show no dialog.
constexpr wchar_t kNoDialogVariable[] = L"MU_CRASH_REPORT_NO_DIALOG";
// Room the crash handler keeps on a thread whose stack overflowed.
constexpr ULONG kStackGuaranteeBytes = 64 * 1024;
constexpr SIZE_T kWriterStackBytes = 512 * 1024;
// The dump takes seconds. Longer means the writer waits for a lock the crashed thread holds (the
// heap's, inside malloc on a damaged heap): then the game closes with the text report only.
constexpr DWORD kTextTimeoutMs = 5 * 1000;
constexpr DWORD kWriteTimeoutMs = 20 * 1000;

struct CrashContext
{
    EXCEPTION_POINTERS* exception = nullptr;
    DWORD threadId = 0;
    HANDLE wake = nullptr;         // set by the crashed thread: write the report
    HANDLE textWritten = nullptr;  // set once the .txt is on disk
    HANDLE written = nullptr;      // set once both files are on disk, before the dialog
    wchar_t dumpPath[kPathCapacity] = {};
    wchar_t textPath[kPathCapacity] = {};
    bool ok = false;
};

constexpr DWORD kHeapCorruptionCode = 0xC0000374;  // STATUS_HEAP_CORRUPTION (ntstatus.h)

LONG s_handling = 0;
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

void ComposeNotice(const CrashContext& crash)
{
    if (!crash.ok)
    {
        std::swprintf(s_notice, kNoticeCapacity,
                      L"MU Skynet закрылась из-за ошибки. Отчёт записать не удалось.\n\n"
                      L"MU Skynet closed because of an error. The crash report could not be written.");
        return;
    }
    const wchar_t* dumpName = crash.dumpPath[0] != L'\0' ? FileName(crash.dumpPath) : L"-";
    std::swprintf(s_notice, kNoticeCapacity,
                  L"MU Skynet закрылась из-за ошибки.\n\n"
                  L"Отчёт сохранён в папке:\n%ls\n\n"
                  L"Пришлите нам в Discord оба файла: %ls и %ls. По ним мы найдём и исправим причину.\n\n"
                  L"MU Skynet closed because of an error. The report is in the folder above; please send us "
                  L"both files on Discord so we can fix it.\n\n"
                  L"Открыть папку? / Open the folder?",
                  Writer::Directory(), FileName(crash.textPath), dumpName);
}

void OpenFolderAt(const wchar_t* path)
{
    wchar_t commandLine[kPathCapacity + 32] = {};
    std::swprintf(commandLine, kPathCapacity + 32, L"explorer.exe /select,\"%ls\"", path);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(nullptr, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
    {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

void ShowNotice(const CrashContext& crash)
{
    if (!s_showDialog)
        return;
    // An exclusive full-screen mode would stay on the monitor and hide the dialog.
    ChangeDisplaySettingsW(nullptr, 0);
    ComposeNotice(crash);
    const UINT style = (crash.ok ? MB_YESNO : MB_OK) | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND | MB_SYSTEMMODAL;
    if (MessageBoxW(nullptr, s_notice, L"MU Skynet", style) == IDYES)
        OpenFolderAt(crash.textPath);
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
    SetEvent(crash->written);
    ShowNotice(*crash);
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
        ShowNotice(s_crash);
    }
    else
    {
        SetEvent(s_crash.wake);
        if (WaitForSingleObject(s_crash.textWritten, kTextTimeoutMs) == WAIT_OBJECT_0 &&
            WaitForSingleObject(s_crash.written, kWriteTimeoutMs) == WAIT_OBJECT_0)
        {
            WaitForSingleObject(s_writer, INFINITE);  // the player reads the dialog
        }
        else
        {
            // The writer waits for a lock this thread holds. Keep its text report, or write one from
            // here; no dump and no dialog, which would wait for the same locks.
            SuspendThread(s_writer);
            if (WaitForSingleObject(s_crash.textWritten, 0) != WAIT_OBJECT_0)
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
// exception by themselves; this raises one so they reach OnUnhandledException with a call stack.
[[noreturn]] void RaiseReportedFailure(const char* what)
{
    const ULONG_PTR arguments[] = {reinterpret_cast<ULONG_PTR>(what)};
    RaiseException(Writer::kReportedFailureCode, EXCEPTION_NONCONTINUABLE, 1, arguments);
    TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
    std::abort();
}

void OnAbortSignal(int)
{
    RaiseReportedFailure("abort() was called");
}

void OnPureCall()
{
    RaiseReportedFailure("pure virtual function call");
}

void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
    RaiseReportedFailure("invalid parameter passed to a C runtime function");
}

// The C runtime's terminate() runs this handler inside __try/__except (abort() included), so an
// exception raised here would be swallowed and the game would end silently. Report from here.
void OnTerminate()
{
    static CONTEXT context;
    static EXCEPTION_RECORD record;
    static EXCEPTION_POINTERS pointers;
    RtlCaptureContext(&context);
    record.ExceptionCode = Writer::kReportedFailureCode;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#if defined(_M_X64)
    record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#elif defined(_M_IX86)
    record.ExceptionAddress = reinterpret_cast<void*>(context.Eip);
#endif
    record.NumberParameters = 1;
    record.ExceptionInformation[0] = reinterpret_cast<ULONG_PTR>(
        "std::terminate() was called (an exception left a noexcept function or a thread)");
    pointers.ExceptionRecord = &record;
    pointers.ContextRecord = &context;
    OnUnhandledException(&pointers);
    TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
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
    return !(length == 1 && value[0] == L'1');
}

} // namespace

void Install()
{
    s_showDialog = DialogWanted();
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
