#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

// The chat macros (Alt+1..Alt+0, set by typing "/1 text".."/0 text"). They live in config.ini ([ChatMacros],
// GameConfig); the old Data\Macro.txt held one macro per line and is read once into config.ini.
namespace UI::Chat::Macros
{
inline constexpr int kSlotCount = 10;
// Characters a macro keeps, as MacroText (ZzzInterface.cpp) without its terminator.
inline constexpr std::size_t kMaxLength = 255;

using MacroSlots = std::array<std::wstring, kSlotCount>;

// The old Macro.txt text: line N is slot N, spaces inside a line are kept, an empty line is an empty slot. Line ends
// (LF or CRLF) are dropped, lines past the tenth are ignored and a line is cut to kMaxLength.
MacroSlots ParseLegacyFile(std::wstring_view text);

// The number on the key of a slot: 1..9 for slots 0..8, 0 for slot 9.
int SlotKeyNumber(int slot);

// A macro as it is kept in config.ini: its UTF-8 bytes, each byte outside 0x21..0x7E and every '%' and '"' written as
// %XX. So spaces at the ends, quotes and any language survive both the Windows ANSI ini and the /play ini.
std::wstring EncodeForIni(std::wstring_view text);

// Back from EncodeForIni. A value with no '%' is itself (macros saved before the encoding); a '%' not followed by
// two hex digits stays a '%'.
std::wstring DecodeFromIni(std::wstring_view value);
} // namespace UI::Chat::Macros
