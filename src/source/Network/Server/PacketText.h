#pragma once

#include <cstddef>
#include <string_view>

// The text inside a received packet: how much of its UTF-8 bytes to convert (Network/Server/WSclient.cpp).
namespace Network::PacketText
{
// The bytes up to the first zero byte, without a UTF-8 character cut off at the end (a lead byte whose
// continuation bytes are missing), so a cut text never ends in a broken character.
std::size_t CompleteUtf8Bytes(std::string_view bytes);
} // namespace Network::PacketText
