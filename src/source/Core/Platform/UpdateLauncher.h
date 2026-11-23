#pragma once

// How the PC client gets a new release: Launcher.exe in the game's folder, started with
// "--wait-pid <the game's process id>", waits until the game has closed and then brings it up to date. A launcher
// older than that option ignores it and just opens.
namespace Core::Platform::UpdateLauncher
{
// Starts it (no console, the game's folder as its working directory) and returns true. False, with a line in
// MuError.log, when there is no Launcher.exe or Windows refused to start it; always false off Windows. The caller
// closes the game either way.
bool Start();
} // namespace Core::Platform::UpdateLauncher
