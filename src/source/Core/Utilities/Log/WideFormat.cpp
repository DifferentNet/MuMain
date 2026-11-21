#include "Core/Utilities/Log/WideFormat.h"

#include <algorithm>
#include <cwchar>
#include <string_view>

namespace Core::Log
{
namespace
{
// What may stand between a % and its conversion: flags, width, precision, an argument number.
constexpr std::wstring_view SpecPrefixChars = L"-+ #0123456789.*$'";
} // namespace

std::wstring NarrowSpecsForMusl(const wchar_t* format)
{
    std::wstring result;
    if (format == nullptr)
        return result;
    const std::wstring_view text(format);
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        result += text[i];
        if (text[i] != L'%')
            continue;
        if (i + 1 < text.size() && text[i + 1] == L'%')
        {
            result += L'%';
            ++i;
            continue;
        }
        std::size_t end = i + 1;
        while (end < text.size() && SpecPrefixChars.find(text[end]) != std::wstring_view::npos)
            ++end;
        if (end + 1 < text.size() && text[end] == L'h' && text[end + 1] == L's')
        {
            result.append(text.substr(i + 1, end - (i + 1)));
            result += L's';
            i = end + 1;
        }
    }
    return result;
}

void WriteUnformatted(wchar_t* buffer, std::size_t count, const wchar_t* format)
{
    if (buffer == nullptr || count == 0)
        return;
    std::wstring line = UnformattedMarker;
    if (format != nullptr)
        line += format;
    const std::size_t length = std::min(line.size(), count - 1);
    std::copy_n(line.data(), length, buffer);
    buffer[length] = L'\0';
}

int FormatLogLine(wchar_t* buffer, std::size_t count, const wchar_t* format, std::va_list args)
{
    if (buffer == nullptr || count == 0)
        return -1;
    buffer[0] = L'\0';
    if (format == nullptr)
        return -1;
#ifdef _WIN32
    const int written = std::vswprintf(buffer, count, format, args);
#else
    const std::wstring narrowed = NarrowSpecsForMusl(format);
    const int written = std::vswprintf(buffer, count, narrowed.c_str(), args);
#endif
    if (written < 0)
    {
        // Too long for the buffer: what fits is kept. Shorter than that: the C library stopped at a conversion it
        // refused (musl writes what came before it, or nothing).
        buffer[count - 1] = L'\0';
        if (std::wcslen(buffer) + 1 < count)
            WriteUnformatted(buffer, count, format);
    }
    return written;
}
} // namespace Core::Log
