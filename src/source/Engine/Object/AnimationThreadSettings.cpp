#include "AnimationThreadSettings.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>

namespace Engine::Object::AnimationThreads
{
namespace
{
// The value without a trailing "; comment", spaces dropped, lower case.
std::wstring Lowered(std::wstring_view text)
{
    text = text.substr(0, text.find(L';'));
    std::wstring lowered;
    for (const wchar_t ch : text)
    {
        if (!std::iswspace(ch)) lowered.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return lowered;
}

bool StartsWithNumber(const std::wstring& value)
{
    std::size_t digit = 0;
    if (!value.empty() && (value[0] == L'+' || value[0] == L'-')) digit = 1;
    return digit < value.size() && std::iswdigit(value[digit]);
}
} // namespace

Mode ParseMode(std::wstring_view text)
{
    const std::wstring value = Lowered(text);
    // The key used to be read as a number (GetPrivateProfileInt: the leading integer, non-zero is on).
    if (StartsWithNumber(value)) return std::wcstol(value.c_str(), nullptr, 10) == 0 ? Mode::Off : Mode::On;
    if (value == L"off" || value == L"false") return Mode::Off;
    if (value == L"on" || value == L"true") return Mode::On;
    return Mode::Auto;
}

Mode ApplyCommandLine(Mode configured, bool enableFlag, bool disableFlag)
{
    if (disableFlag) return Mode::Off;
    if (enableFlag) return Mode::On;
    return configured;
}

std::size_t WorkerCount(Mode mode, unsigned hardwareThreads)
{
    if (mode == Mode::Off) return 0;
    if (mode == Mode::Auto && hardwareThreads < MinAutoHardwareThreads) return 0;

    const int spare = static_cast<int>(hardwareThreads / 2) - 2;
    return static_cast<std::size_t>(std::clamp(spare, 1, static_cast<int>(MaxWorkers)));
}
} // namespace Engine::Object::AnimationThreads
