#pragma once

// Web build only. The browser can't load MUnique.Client.Library (the C# network library of the
// desktop client), so the web build has the same functions in C++: connections over WebSockets to
// the server's WebSocket bridge (scripts/serve-web.py locally), OpenMU's packet encryption
// (MuPacketCrypto), and the Send<Packet> functions generated from OpenMU's packet definitions
// (tools/web/gen_web_packets.py). The client finds them by name, as it does in the library:
// Core/Platform/posix/PlatformLibrary.cpp hands them out for "MUnique.Client.Library".

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mu::web
{
struct NetworkSymbol
{
    const char* name;
    void* function;
};

// The library's exports by name: connection functions, the hand-written sends and the generated ones.
void* FindNetworkSymbol(const char* name);

// The generated Send<Packet> functions (WebPacketFunctions.generated.cpp).
const std::vector<NetworkSymbol>& GeneratedPacketFunctions();

// The page bridge's WebSocket address for a game server port: ws/<port> beside the page.
std::string BridgeUrl(std::uint16_t port);

// Sends complete client packets on a connection, encrypted when the connection is.
void SendPacket(std::int32_t handle, const std::uint8_t* data, std::size_t size);
} // namespace mu::web
