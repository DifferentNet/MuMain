#include "stdafx.h"
#include "Core/Platform/ExecutableFolder.h"

#include <string>

namespace Core::Platform
{
namespace
{
// As the crash report's: GetModuleFileNameW cuts a longer path off without saying so (it then fills the buffer).
constexpr DWORD ExePathCapacity = MAX_PATH * 2;
} // namespace

std::filesystem::path ExecutableFolder()
{
    std::wstring exePath(ExePathCapacity, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, exePath.data(), ExePathCapacity);
    if (length == 0 || length >= ExePathCapacity)
        return {};

    exePath.resize(length);
    return std::filesystem::path(exePath).parent_path();
}
} // namespace Core::Platform
