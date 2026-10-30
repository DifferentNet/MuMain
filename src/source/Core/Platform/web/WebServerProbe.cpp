// Web build only: see WebServerProbe.h.

#include "Core/Platform/web/WebServerProbe.h"

#include "Core/Platform/web/WebNetwork.h"

#include <emscripten/websocket.h>

#include <cstdint>
#include <map>
#include <string>

namespace
{
using mu::web::ServerProbeResult;

struct Probe
{
    EMSCRIPTEN_WEBSOCKET_T socket = 0;
    ServerProbeResult result = ServerProbeResult::Waiting;
};

std::map<std::int32_t, Probe> g_probes;
std::int32_t g_nextProbe = 1;

void Settle(void* userData, ServerProbeResult result)
{
    const auto it = g_probes.find(static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(userData)));
    if (it != g_probes.end() && it->second.result == ServerProbeResult::Waiting)
    {
        it->second.result = result;
    }
}

EM_BOOL OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* userData)
{
    Settle(userData, ServerProbeResult::Up);
    return EM_TRUE;
}

EM_BOOL OnError(int, const EmscriptenWebSocketErrorEvent*, void* userData)
{
    Settle(userData, ServerProbeResult::Down);
    return EM_TRUE;
}

EM_BOOL OnClose(int, const EmscriptenWebSocketCloseEvent*, void* userData)
{
    Settle(userData, ServerProbeResult::Down);
    return EM_TRUE;
}
} // namespace

std::int32_t mu::web::StartServerProbe(std::uint16_t port)
{
    if (!emscripten_websocket_is_supported())
    {
        return 0;
    }

    const std::string url = BridgeUrl(port);
    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = url.c_str();
    attributes.protocols = "binary";
    attributes.createOnMainThread = EM_TRUE;
    const EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attributes);
    if (socket <= 0)
    {
        return 0;
    }

    const std::int32_t id = g_nextProbe++;
    g_probes[id].socket = socket;
    void* userData = reinterpret_cast<void*>(static_cast<std::intptr_t>(id));
    emscripten_websocket_set_onopen_callback(socket, userData, OnOpen);
    emscripten_websocket_set_onerror_callback(socket, userData, OnError);
    emscripten_websocket_set_onclose_callback(socket, userData, OnClose);
    return id;
}

ServerProbeResult mu::web::PollServerProbe(std::int32_t probe)
{
    const auto it = g_probes.find(probe);
    return it != g_probes.end() ? it->second.result : ServerProbeResult::Down;
}

void mu::web::CloseServerProbe(std::int32_t probe)
{
    const auto it = g_probes.find(probe);
    if (it == g_probes.end())
    {
        return;
    }

    constexpr unsigned short NormalClosure = 1000;
    emscripten_websocket_close(it->second.socket, NormalClosure, "");
    emscripten_websocket_delete(it->second.socket);
    g_probes.erase(it);
}
