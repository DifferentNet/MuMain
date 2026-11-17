// MU Moldova: numbers in chat and dialog lines with thousands separators ("Obtained 1,234,567 Exp").
// The localized format keeps its %d / %lu placeholders; each integer placeholder is replaced in order
// by the value grouped with commas as ConvertGold64 writes Zen, so the .resx strings stay unchanged.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>

namespace UI::Text
{
// The format with every integer placeholder (%d, %i, %u with any l/ll/I64 length) replaced, in order,
// by the next value grouped by thousands, and "%%" written as "%". A placeholder past the last value,
// or of another kind, stays as written.
[[nodiscard]] std::wstring FormatGroupedNumbers(const wchar_t* format, std::initializer_list<std::int64_t> values);

// The value grouped by thousands: 1234567 -> "1,234,567", -1500 -> "-1,500".
[[nodiscard]] std::wstring GroupThousands(std::int64_t value);
} // namespace UI::Text
