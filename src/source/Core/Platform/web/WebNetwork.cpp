// Web build only: see WebNetwork.h.

#include "Core/Platform/web/WebNetwork.h"

#include "Core/Platform/web/MuPacketCrypto.h"
#include "WebPacketBuilders.generated.h"

#include <emscripten.h>
#include <emscripten/websocket.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>

namespace
{
using OnPacketReceived = void (*)(std::int32_t handle, std::int32_t size, std::uint8_t* data);
using OnDisconnected = void (*)(std::int32_t handle);
using LogCallback = void (*)(std::uint8_t level, const char* message);

enum class LogLevel : std::uint8_t
{
    Debug,
    Info,
    Warning,
    Error,
};

constexpr int ConnectTimeoutMs = 10000;

struct WebConnection
{
    explicit WebConnection(bool isEncrypted) : encrypted(isEncrypted), decryptor(isEncrypted)
    {
    }

    EMSCRIPTEN_WEBSOCKET_T socket = 0;
    bool encrypted;
    bool open = false;
    bool closed = false;
    bool receiving = false;    // BeginReceive called: deliver packets
    bool disconnectedReported = false;
    mu::web::MuPacketEncryptor encryptor;
    mu::web::MuPacketDecryptor decryptor;
    std::vector<std::vector<std::uint8_t>> pendingPackets; // arrived before BeginReceive
    OnPacketReceived onPacketReceived = nullptr;
    OnDisconnected onDisconnected = nullptr;
};

std::map<std::int32_t, std::unique_ptr<WebConnection>> g_connections;
std::int32_t g_nextHandle = 1;
LogCallback g_logCallback = nullptr;

void Log(LogLevel level, const std::string& message)
{
    if (g_logCallback != nullptr)
    {
        g_logCallback(static_cast<std::uint8_t>(level), message.c_str());
    }
    else
    {
        std::printf("NET: %s\n", message.c_str());
    }
}

WebConnection* Find(std::int32_t handle)
{
    const auto it = g_connections.find(handle);
    return it != g_connections.end() ? it->second.get() : nullptr;
}

// The page's WebSocket bridge: ws/<port> beside the page on the server that served it (ws or wss
// like it), so the game also runs from a folder: https://skynet-mu.net/play/ uses /play/ws/<port>.
EM_JS(char*, MuWeb_BridgeBaseUrl, (), {
    const scheme = location.protocol === "https:" ? "wss:" : "ws:";
    const path = location.pathname;
    const folder = path.substring(0, path.lastIndexOf("/") + 1);
    return stringToNewUTF8(scheme + "//" + location.host + folder + "ws/");
});

void ReportDisconnected(std::int32_t handle, WebConnection* connection)
{
    if (connection->disconnectedReported)
    {
        return;
    }
    connection->disconnectedReported = true;
    if (connection->onDisconnected != nullptr)
    {
        connection->onDisconnected(handle);
    }
}

// index.html?packets: every packet sent and received, unencrypted, in the page's log.
EM_JS(int, MuWeb_LogPackets, (), { return new URLSearchParams(location.search).has("packets") ? 1 : 0; });

void LogPacket(const char* direction, std::int32_t handle, const std::uint8_t* data, std::size_t size)
{
    std::string text = std::string(direction) + " " + std::to_string(handle) + ":";
    char hex[4];
    for (std::size_t i = 0; i < size && i < 64; ++i)
    {
        std::snprintf(hex, sizeof(hex), " %02X", data[i]);
        text += hex;
    }
    if (size > 64)
    {
        text += " ... (" + std::to_string(size) + " bytes)";
    }
    std::printf("NET %s\n", text.c_str());
}

void Deliver(std::int32_t handle, WebConnection* connection, std::vector<std::uint8_t>&& packet)
{
    if (MuWeb_LogPackets())
    {
        LogPacket("recv", handle, packet.data(), packet.size());
    }
    if (!connection->receiving)
    {
        connection->pendingPackets.push_back(std::move(packet));
        return;
    }
    connection->onPacketReceived(handle, static_cast<std::int32_t>(packet.size()), packet.data());
}

EM_BOOL OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* userData)
{
    if (WebConnection* connection = Find(static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(userData))))
    {
        connection->open = true;
    }
    return EM_TRUE;
}

EM_BOOL OnMessage(int, const EmscriptenWebSocketMessageEvent* event, void* userData)
{
    const auto handle = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(userData));
    WebConnection* connection = Find(handle);
    if (connection == nullptr || connection->closed || event->isText)
    {
        return EM_TRUE;
    }
    std::vector<std::vector<std::uint8_t>> packets;
    const bool valid = connection->decryptor.Feed(event->data, event->numBytes,
        [&](const std::uint8_t* packet, std::size_t size) { packets.emplace_back(packet, packet + size); });
    // Handed over after Feed: a packet handler may disconnect, which frees the connection.
    for (auto& packet : packets)
    {
        connection = Find(handle);
        if (connection == nullptr || connection->closed)
        {
            return EM_TRUE;
        }
        Deliver(handle, connection, std::move(packet));
    }
    connection = Find(handle);
    if (!valid && connection != nullptr && !connection->closed)
    {
        Log(LogLevel::Error, "Malformed data from the server, disconnecting (handle " + std::to_string(handle) + ")");
        connection->closed = true;
        emscripten_websocket_close(connection->socket, 1002, "malformed data");
        ReportDisconnected(handle, connection);
    }
    return EM_TRUE;
}

EM_BOOL OnClose(int, const EmscriptenWebSocketCloseEvent*, void* userData)
{
    const auto handle = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(userData));
    if (WebConnection* connection = Find(handle))
    {
        connection->closed = true;
        if (connection->open)
        {
            ReportDisconnected(handle, connection);
        }
    }
    return EM_TRUE;
}

EM_BOOL OnError(int, const EmscriptenWebSocketErrorEvent*, void* userData)
{
    const auto handle = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(userData));
    if (WebConnection* connection = Find(handle))
    {
        connection->closed = true;
        if (connection->open)
        {
            ReportDisconnected(handle, connection);
        }
    }
    return EM_TRUE;
}

// The packet header says how long a packet is: C1/C3 one byte, C2/C4 two (big endian).
std::size_t PacketLength(const std::uint8_t* data, std::size_t available)
{
    if (available < 3)
    {
        return 0;
    }
    switch (data[0])
    {
    case 0xC1:
    case 0xC3:
        return data[1];
    case 0xC2:
    case 0xC4:
        return (static_cast<std::size_t>(data[1]) << 8) | data[2];
    default:
        return 0;
    }
}

// --- The library's connection functions (ClientLibrary/ConnectionManager.cs) --------------------

void ConnectionManager_SetLogCallback(LogCallback callback)
{
    g_logCallback = callback;
}

std::int32_t ConnectionManager_Connect(const char16_t* host, std::int32_t port, std::uint8_t isEncrypted,
                                       OnPacketReceived onPacketReceived, OnDisconnected onDisconnected)
{
    if (!emscripten_websocket_is_supported())
    {
        Log(LogLevel::Error, "This browser has no WebSockets");
        return -1;
    }

    // The bridge connects to the game server itself: the host the client asks for (often a
    // loopback address in the connect server's reply) isn't reachable from the browser anyway.
    const std::string url = mu::web::BridgeUrl(static_cast<std::uint16_t>(port));

    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = url.c_str();
    attributes.protocols = "binary";
    attributes.createOnMainThread = EM_TRUE;
    const EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attributes);
    if (socket <= 0)
    {
        Log(LogLevel::Error, "Can't open a WebSocket to " + url);
        return -1;
    }

    const std::int32_t handle = g_nextHandle++;
    auto connection = std::make_unique<WebConnection>(isEncrypted == 1);
    connection->socket = socket;
    connection->onPacketReceived = onPacketReceived;
    connection->onDisconnected = onDisconnected;
    WebConnection* raw = connection.get();
    g_connections[handle] = std::move(connection);

    void* userData = reinterpret_cast<void*>(static_cast<std::intptr_t>(handle));
    emscripten_websocket_set_onopen_callback(socket, userData, OnOpen);
    emscripten_websocket_set_onmessage_callback(socket, userData, OnMessage);
    emscripten_websocket_set_onclose_callback(socket, userData, OnClose);
    emscripten_websocket_set_onerror_callback(socket, userData, OnError);

    // The library's Connect returns once the connection stands, so this one waits for it too,
    // handing control to the browser meanwhile.
    for (int waited = 0; !raw->open && !raw->closed && waited < ConnectTimeoutMs; waited += 10)
    {
        emscripten_sleep(10);
    }
    if (!raw->open)
    {
        Log(LogLevel::Error, "Connection to " + url + " failed");
        raw->closed = true;
        emscripten_websocket_close(socket, 1000, "");
        emscripten_websocket_delete(socket);
        g_connections.erase(handle);
        return -1;
    }

    Log(LogLevel::Info, "Connected to " + url + " (handle " + std::to_string(handle) +
                            (isEncrypted == 1 ? ", encrypted)" : ")"));
    return handle;
}

void ConnectionManager_BeginReceive(std::int32_t handle)
{
    WebConnection* connection = Find(handle);
    if (connection == nullptr || connection->receiving)
    {
        return;
    }
    connection->receiving = true;
    auto pending = std::move(connection->pendingPackets);
    for (auto& packet : pending)
    {
        connection->onPacketReceived(handle, static_cast<std::int32_t>(packet.size()), packet.data());
    }
}

void ConnectionManager_Send(std::int32_t handle, const std::uint8_t* data, std::int32_t count)
{
    mu::web::SendPacket(handle, data, static_cast<std::size_t>(count));
}

void ConnectionManager_Disconnect(std::int32_t handle)
{
    WebConnection* connection = Find(handle);
    if (connection == nullptr)
    {
        return;
    }
    if (!connection->closed)
    {
        connection->closed = true;
        emscripten_websocket_close(connection->socket, 1000, "");
    }
    // Reported right away: the client frees its connection object after this call.
    ReportDisconnected(handle, connection);
    emscripten_websocket_delete(connection->socket);
    g_connections.erase(handle);
}

// --- The library's hand-written sends (ClientLibrary/ConnectionManager.*.Custom.cs) ------------

constexpr std::uint8_t Xor3Keys[3] = {0xFC, 0xCF, 0xAB};

// OpenMU's Xor3Encryptor: from `start`, each byte XOR the key byte of its distance from `start`.
void Xor3(std::uint8_t* data, std::size_t size, std::size_t start)
{
    for (std::size_t i = start; i < size; ++i)
    {
        data[i] ^= Xor3Keys[(i - start) % 3];
    }
}

void ConnectionManager_SendLogin(std::int32_t handle, const char16_t* username, const char16_t* password,
                                 std::uint32_t tickCount, const std::uint8_t* clientVersion,
                                 const std::uint8_t* clientSerial)
{
    const std::string user = mu::web::packets::ToUtf8(username);
    const std::string pass = mu::web::packets::ToUtf8(password);
    std::uint8_t userBytes[10] = {};
    std::uint8_t passBytes[20] = {};
    if (user.size() > sizeof(userBytes) || pass.size() > sizeof(passBytes) || clientVersion == nullptr ||
        clientSerial == nullptr)
    {
        Log(LogLevel::Error, "Login not sent: the credentials don't fit the packet");
        return;
    }
    std::memcpy(userBytes, user.data(), user.size());
    std::memcpy(passBytes, pass.data(), pass.size());
    Xor3(userBytes, sizeof(userBytes), 0);
    Xor3(passBytes, sizeof(passBytes), 0);
    const auto packet = mu::web::packets::BuildLoginLongPassword(userBytes, sizeof(userBytes), passBytes,
                                                                sizeof(passBytes), tickCount, clientVersion, 5,
                                                                clientSerial, 16);
    mu::web::SendPacket(handle, packet.data(), packet.size());
}

void ConnectionManager_SendAuthenticateExt(std::int32_t handle, std::uint16_t roomId, std::uint32_t token)
{
    const std::string tokenText = std::to_string(token);
    auto packet = mu::web::packets::BuildAuthenticate(
        roomId, reinterpret_cast<const std::uint8_t*>(tokenText.data()), static_cast<std::uint32_t>(tokenText.size()));
    Xor3(packet.data(), packet.size(), 6);
    mu::web::SendPacket(handle, packet.data(), packet.size());
}

void ConnectionManager_SendChatMessageExt(std::int32_t handle, std::uint8_t senderIndex, const char16_t* message)
{
    const std::string text = mu::web::packets::ToUtf8(message);
    auto packet = mu::web::packets::BuildChatMessage(senderIndex, static_cast<std::uint8_t>(text.size()),
                                                     reinterpret_cast<const std::uint8_t*>(text.data()),
                                                     static_cast<std::uint32_t>(text.size()));
    Xor3(packet.data(), packet.size(), 5);
    mu::web::SendPacket(handle, packet.data(), packet.size());
}

const mu::web::NetworkSymbol ConnectionFunctions[] = {
    {"ConnectionManager_SetLogCallback", reinterpret_cast<void*>(&ConnectionManager_SetLogCallback)},
    {"ConnectionManager_Connect", reinterpret_cast<void*>(&ConnectionManager_Connect)},
    {"ConnectionManager_BeginReceive", reinterpret_cast<void*>(&ConnectionManager_BeginReceive)},
    {"ConnectionManager_Send", reinterpret_cast<void*>(&ConnectionManager_Send)},
    {"ConnectionManager_Disconnect", reinterpret_cast<void*>(&ConnectionManager_Disconnect)},
    {"ConnectionManager_SendLogin", reinterpret_cast<void*>(&ConnectionManager_SendLogin)},
    {"ConnectionManager_SendAuthenticateExt", reinterpret_cast<void*>(&ConnectionManager_SendAuthenticateExt)},
    {"ConnectionManager_SendChatMessageExt", reinterpret_cast<void*>(&ConnectionManager_SendChatMessageExt)},
};
} // namespace

void mu::web::SendPacket(std::int32_t handle, const std::uint8_t* data, std::size_t size)
{
    WebConnection* connection = Find(handle);
    if (connection == nullptr || !connection->open || connection->closed || data == nullptr)
    {
        return;
    }

    // One call can carry several packets; each is encrypted on its own, as the library's pipe does.
    for (std::size_t offset = 0; offset < size;)
    {
        std::size_t length = PacketLength(data + offset, size - offset);
        if (length == 0 || length > size - offset)
        {
            Log(LogLevel::Error, "Not sent: malformed outgoing packet (handle " + std::to_string(handle) + ")");
            return;
        }
        if (MuWeb_LogPackets())
        {
            LogPacket("send", handle, data + offset, length);
        }
        if (connection->encrypted)
        {
            std::vector<std::uint8_t> encrypted = connection->encryptor.Encrypt(data + offset, length);
            if (encrypted.empty())
            {
                // The C# library breaks the connection on a packet it can't encrypt.
                Log(LogLevel::Error, "Can't encrypt an outgoing packet, disconnecting (handle " + std::to_string(handle) + ")");
                connection->closed = true;
                emscripten_websocket_close(connection->socket, 1000, "");
                ReportDisconnected(handle, connection);
                return;
            }
            emscripten_websocket_send_binary(connection->socket, encrypted.data(), static_cast<std::uint32_t>(encrypted.size()));
        }
        else
        {
            emscripten_websocket_send_binary(connection->socket, const_cast<std::uint8_t*>(data + offset),
                                             static_cast<std::uint32_t>(length));
        }
        offset += length;
    }
}

std::string mu::web::BridgeUrl(std::uint16_t port)
{
    char* base = MuWeb_BridgeBaseUrl();
    std::string url = std::string(base) + std::to_string(port);
    std::free(base);
    return url;
}

void* mu::web::FindNetworkSymbol(const char* name)
{
    for (const NetworkSymbol& symbol : ConnectionFunctions)
    {
        if (std::strcmp(symbol.name, name) == 0)
        {
            return symbol.function;
        }
    }
    for (const NetworkSymbol& symbol : GeneratedPacketFunctions())
    {
        if (std::strcmp(symbol.name, name) == 0)
        {
            return symbol.function;
        }
    }
    return nullptr;
}
