#include "stdafx.h"
#include "Core/Platform/UpdateLauncher.h"

#include "Core/Utilities/Log/MuLogger.h"

#ifdef _WIN32
#include "Core/Platform/ExecutableFolder.h"
#include "Core/Utilities/StringUtils.h"

#include <filesystem>
#include <string>
#include <system_error>
#endif

namespace Core::Platform::UpdateLauncher
{
#ifdef _WIN32
namespace
{
constexpr wchar_t LauncherFileName[] = L"Launcher.exe";
constexpr wchar_t WaitPidOption[] = L"--wait-pid";

// "<launcher>" --wait-pid <this process>. CreateProcessW may write into the command line, so it is its own string.
std::wstring CommandLine(const std::filesystem::path& launcher)
{
    return L"\"" + launcher.wstring() + L"\" " + WaitPidOption + L" " + std::to_wstring(GetCurrentProcessId());
}
} // namespace

bool Start()
{
    const std::filesystem::path folder = Core::Platform::ExecutableFolder();
    const std::filesystem::path launcher = folder / LauncherFileName;
    std::error_code error;
    if (folder.empty() || !std::filesystem::is_regular_file(launcher, error))
    {
        mu::log::Get("platform")->error("UPDATE: no launcher at '{}', the game closes without it",
                                        StringUtils::WideToNarrow(launcher.c_str()));
        return false;
    }

    std::wstring commandLine = CommandLine(launcher);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    // No creation flags: Launcher.exe is a window program (WinExe), so no console opens for it.
    if (!CreateProcessW(launcher.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(),
                        &startup, &process))
    {
        const DWORD startError = GetLastError();
        mu::log::Get("platform")->error("UPDATE: the launcher '{}' did not start (error {}), the game closes without it",
                                        StringUtils::WideToNarrow(launcher.c_str()), startError);
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    mu::log::Get("platform")->info("UPDATE: started '{}', it waits for this game to close",
                                   StringUtils::WideToNarrow(commandLine.c_str()));
    return true;
}
#else
bool Start()
{
    // The launcher is a Windows program: elsewhere the player updates the game by hand.
    mu::log::Get("platform")->info("UPDATE: no launcher off Windows, the game closes without it");
    return false;
}
#endif
} // namespace Core::Platform::UpdateLauncher
