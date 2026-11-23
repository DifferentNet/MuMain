#pragma once

#include <filesystem>

namespace Core::Platform
{
// The folder Main.exe runs from (GetModuleFileNameW), whatever the working directory is. Empty when the system
// gives no path or the path is too long for its buffer.
std::filesystem::path ExecutableFolder();
} // namespace Core::Platform
