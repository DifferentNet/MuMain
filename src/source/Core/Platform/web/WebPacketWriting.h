#pragma once

// Web build only: what the generated packet builders (WebPacketBuilders.generated.h, from
// tools/web/gen_web_packets.py) use to write fields. Each helper writes bytes the way OpenMU's
// packet structs do (MUnique.OpenMU.Network.Packets, ByteSpanExtensions).

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mu::web::packets
{
// OpenMU's SetByteValue: `bits` bits of value, shifted left by `leftShifted`, other bits kept.
inline void SetByteValue(std::uint8_t* target, std::uint8_t value, int bits, int leftShifted)
{
    const auto bitMask = static_cast<std::uint8_t>(((1u << bits) - 1u) << leftShifted);
    *target = static_cast<std::uint8_t>((*target & ~bitMask) | ((value << leftShifted) & bitMask));
}

inline void SetBoolean(std::uint8_t* target, bool value, int leftShifted)
{
    const auto mask = static_cast<std::uint8_t>(1u << leftShifted);
    *target = static_cast<std::uint8_t>(value ? (*target | mask) : (*target & ~mask));
}

inline void WriteUInt16LittleEndian(std::uint8_t* target, std::uint16_t value)
{
    target[0] = static_cast<std::uint8_t>(value);
    target[1] = static_cast<std::uint8_t>(value >> 8);
}

inline void WriteUInt16BigEndian(std::uint8_t* target, std::uint16_t value)
{
    target[0] = static_cast<std::uint8_t>(value >> 8);
    target[1] = static_cast<std::uint8_t>(value);
}

inline void WriteUInt32LittleEndian(std::uint8_t* target, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i)
    {
        target[i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

inline void WriteUInt32BigEndian(std::uint8_t* target, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i)
    {
        target[i] = static_cast<std::uint8_t>(value >> (8 * (3 - i)));
    }
}

inline void WriteUInt64LittleEndian(std::uint8_t* target, std::uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        target[i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

inline void WriteUInt64BigEndian(std::uint8_t* target, std::uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        target[i] = static_cast<std::uint8_t>(value >> (8 * (7 - i)));
    }
}

inline void WriteFloat(std::uint8_t* target, float value)
{
    std::memcpy(target, &value, sizeof(value)); // little endian, as BitConverter writes it
}

inline void WriteDouble(std::uint8_t* target, double value)
{
    std::memcpy(target, &value, sizeof(value));
}

// OpenMU clears the field and writes the UTF-8 bytes. It throws for text longer than a fixed
// field (and the packet isn't sent); here the text is cut off instead.
inline void WriteString(std::uint8_t* target, std::size_t fieldSize, const std::string& utf8)
{
    std::memset(target, 0, fieldSize);
    std::memcpy(target, utf8.data(), utf8.size() < fieldSize ? utf8.size() : fieldSize);
}

inline void WriteBinary(std::uint8_t* target, std::size_t fieldSize, const std::uint8_t* data, std::uint32_t size)
{
    if (data != nullptr)
    {
        std::memcpy(target, data, size < fieldSize ? size : fieldSize);
    }
}

// The client passes text as UTF-16 (wchar_t on Windows); the packets carry UTF-8.
// A surrogate without its partner becomes U+FFFD, as .NET's Encoding.UTF8 writes it in the desktop
// library (tools/web/packet-test checks this).
inline std::string ToUtf8(const char16_t* text)
{
    constexpr std::uint32_t ReplacementCharacter = 0xFFFD;
    std::string result;
    if (text == nullptr)
    {
        return result;
    }
    for (const char16_t* c = text; *c != 0; ++c)
    {
        std::uint32_t code = *c;
        if (code >= 0xD800 && code <= 0xDBFF && c[1] >= 0xDC00 && c[1] <= 0xDFFF)
        {
            code = 0x10000 + ((code - 0xD800) << 10) + (c[1] - 0xDC00);
            ++c;
        }
        else if (code >= 0xD800 && code <= 0xDFFF)
        {
            code = ReplacementCharacter;
        }
        if (code < 0x80)
        {
            result += static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            result += static_cast<char>(0xC0 | (code >> 6));
            result += static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            result += static_cast<char>(0xE0 | (code >> 12));
            result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            result += static_cast<char>(0xF0 | (code >> 18));
            result += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
    return result;
}
} // namespace mu::web::packets
