#pragma once

// Web build only. Whether the game server answers, for auto-reconnect
// (Network/Reconnect/ReconnectManager.cpp). A browser can't open the TCP socket the desktop client
// probes with, so this opens the page bridge's WebSocket for the server's port. The bridge
// (server/web/serve-web.py) accepts it only after the game server took its connection, so a socket
// that opens means the server is up.

#include <cstdint>

namespace mu::web
{
enum class ServerProbeResult
{
    Waiting,
    Up,
    Down,
};

// Starts a probe of the game server on `port`; 0 when the browser couldn't start one.
std::int32_t StartServerProbe(std::uint16_t port);

// Waiting until the bridge answers; Down when it refused or the socket failed.
ServerProbeResult PollServerProbe(std::int32_t probe);

// Closes the probe's socket and forgets it; harmless for an unknown probe.
void CloseServerProbe(std::int32_t probe);
} // namespace mu::web
