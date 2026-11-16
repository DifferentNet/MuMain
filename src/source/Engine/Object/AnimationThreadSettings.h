#pragma once

#include <cstddef>
#include <string_view>

// How many worker threads animate the characters ([UI] EnableAnimationTaskPool, --enable-taskpool,
// --disable-taskpool).
namespace Engine::Object::AnimationThreads
{
enum class Mode
{
    Auto, // workers on a PC with at least MinAutoHardwareThreads hardware threads
    Off,  // the main thread animates every character
    On,   // workers whatever the PC
};

inline constexpr unsigned MinAutoHardwareThreads = 4;
inline constexpr std::size_t MaxWorkers = 4;

// "off"/"false" and "on"/"true" (any case); a number, as the key was read before: 0 is Off, any other
// is On; anything else, or nothing, is Auto. A trailing "; comment" is ignored.
Mode ParseMode(std::wstring_view text);

// The config value, then the command line: --disable-taskpool wins over --enable-taskpool.
Mode ApplyCommandLine(Mode configured, bool enableFlag, bool disableFlag);

// 0 = no workers. Otherwise half the hardware threads less two (the main and render work keep the
// rest), at least 1 and at most MaxWorkers.
std::size_t WorkerCount(Mode mode, unsigned hardwareThreads);
} // namespace Engine::Object::AnimationThreads
