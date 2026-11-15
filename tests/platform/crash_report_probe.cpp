// Installs the crash reporter (Core/Platform/CrashReport.h) and crashes the way argv[1] names,
// for test_crash_report.py. Exits 0 only when asked to not crash.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>

#include <windows.h>

#include "Core/Platform/CrashReport.h"

namespace
{

struct Base
{
    Base() { CallPure(); }
    virtual ~Base() = default;
    virtual void Pure() = 0;
    void CallPure() { Pure(); }
};

struct Derived : Base
{
    void Pure() override {}
};

int Recurse(volatile int depth)
{
    volatile char padding[4096];
    padding[0] = static_cast<char>(depth);
    return Recurse(depth + 1) + padding[0];
}

// Frees one block twice on a heap that ends the process on corruption (STATUS_HEAP_CORRUPTION).
void CorruptHeap()
{
    HANDLE heap = HeapCreate(0, 0, 0);
    HeapSetInformation(heap, HeapEnableTerminationOnCorruption, nullptr, 0);
    void* block = HeapAlloc(heap, 0, 64);
    HeapFree(heap, 0, block);
    HeapFree(heap, 0, block);
}

void ThrowFromNoexcept() noexcept
{
    throw std::runtime_error("from a noexcept function");
}

} // namespace

int main(int argc, char** argv)
{
    if (Core::Platform::CrashReport::RunNoticeIfAsked())
        return 0;
    Core::Platform::CrashReport::Install();
    const char* kind = argc > 1 ? argv[1] : "none";

    if (std::strcmp(kind, "access-violation") == 0)
        *static_cast<volatile int*>(nullptr) = 1;
    else if (std::strcmp(kind, "stack-overflow") == 0)
        std::printf("%d\n", Recurse(0));
    else if (std::strcmp(kind, "cpp-exception") == 0)
        throw std::runtime_error("uncaught");
    else if (std::strcmp(kind, "abort") == 0)
        std::abort();
    else if (std::strcmp(kind, "pure-call") == 0)
        Derived derived;
    else if (std::strcmp(kind, "terminate") == 0)
        ThrowFromNoexcept();
    else if (std::strcmp(kind, "heap-corruption") == 0)
        CorruptHeap();
    else if (std::strcmp(kind, "heap-locked") == 0)
    {
        // A crash while this thread holds the heap lock (inside malloc on a damaged heap).
        HeapLock(GetProcessHeap());
        *static_cast<volatile int*>(nullptr) = 1;
    }
    else if (std::strcmp(kind, "thread-exception") == 0)
    {
        // An exception that escapes a thread: std::terminate() on that thread, not the main one.
        std::thread([] { throw std::runtime_error("from a thread"); }).join();
    }
    else if (std::strcmp(kind, "folder-gone") == 0)
    {
        // The player deleted CrashReports while the game ran (it is empty in the test).
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring folder(exe);
        folder.resize(folder.find_last_of(L'\\') + 1);
        RemoveDirectoryW((folder + L"CrashReports").c_str());
        *static_cast<volatile int*>(nullptr) = 1;
    }
    return 0;
}
