// Needs nothing of stdafx.h, so the unit test (tests/network/test_packet_text.cpp) compiles it alone.
#include "Network/Server/PacketText.h"

#include <algorithm>

namespace
{
constexpr unsigned char ContinuationMask = 0xC0;
constexpr unsigned char ContinuationBits = 0x80;
// A character has at most four bytes, so at most three continuation bytes follow its lead byte.
constexpr std::size_t MaxContinuationBytes = 3;

bool IsContinuation(unsigned char byte)
{
    return (byte & ContinuationMask) == ContinuationBits;
}

// How many bytes the character starting with this lead byte has; 1 for a plain or broken byte.
std::size_t SequenceLength(unsigned char lead)
{
    if ((lead & 0xE0) == 0xC0)
    {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0)
    {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0)
    {
        return 4;
    }
    return 1;
}
} // namespace

namespace Network::PacketText
{
std::size_t CompleteUtf8Bytes(std::string_view bytes)
{
    const std::size_t length = std::min(bytes.find('\0'), bytes.size());
    std::size_t lead = length;
    std::size_t continuations = 0;
    while (lead > 0 && continuations <= MaxContinuationBytes && IsContinuation(static_cast<unsigned char>(bytes[lead - 1])))
    {
        --lead;
        ++continuations;
    }
    if (lead == 0 || continuations > MaxContinuationBytes)
    {
        return length;
    }
    const std::size_t start = lead - 1;
    const std::size_t needed = SequenceLength(static_cast<unsigned char>(bytes[start]));
    return length - start < needed ? start : length;
}
} // namespace Network::PacketText
