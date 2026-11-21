#pragma once

#include <cstdarg>
#include <cstddef>
#include <string>

namespace Core::Log
{
// What a log line starts with when the C library refused its format whole (FormatLogLine()): the format follows
// as it is, without its arguments, so the line still says where it came from instead of being lost.
inline constexpr wchar_t UnformattedMarker[] = L"[unformatted] ";

// `format` with each %hs as %s, its flags, width, precision and argument number kept (%-8hs, %.64hs, %1$hs);
// %%hs, a plain percent sign and "hs", stays. %hs is the narrow string in MSVC's wide printf; in the others'
// (musl in the web build) the narrow string is %s, and %hs is no conversion at all: musl refused the whole format
// and wrote nothing, so the line was lost. Empty for a null format.
std::wstring NarrowSpecsForMusl(const wchar_t* format);

// UnformattedMarker and `format` in `buffer` (`count` wide characters), cut to fit and ended.
void WriteUnformatted(wchar_t* buffer, std::size_t count, const wchar_t* format);

// A log line from `format` and `args` in `buffer` (`count` wide characters), always ended: vswprintf, with
// NarrowSpecsForMusl()'s format outside Windows. A line longer than the buffer keeps what fits; one whose format
// the C library refused (all of it, or from a conversion on) gets WriteUnformatted()'s line. Returns what
// vswprintf returned.
int FormatLogLine(wchar_t* buffer, std::size_t count, const wchar_t* format, std::va_list args);
} // namespace Core::Log
