#pragma once

// Web build only. The desktop client sends and receives through the C# ClientLibrary, which encrypts
// with OpenMU's network library (MUnique.OpenMU.Network); the browser can't run it, so this is the
// same encryption in C++. tools/web/crypto-test checks it byte for byte against OpenMU's own classes.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mu::web
{
// Client-to-server encryption for one connection: Xor32, then SimpleModulus for C3/C4 packets
// (OpenMU's PipelinedXor32Encryptor over PipelinedSimpleModulusEncryptor, default client key).
// Keeps the per-connection packet counter.
class MuPacketEncryptor
{
public:
    // packet: one complete plain packet (C1/C2/C3/C4 header, length matching its header).
    // Like ConnectionManager_Send, it first writes size into the header's length field.
    // Returns the bytes to put on the wire. Returns nothing for a packet the C# library couldn't send
    // either (another first byte, shorter than 3 bytes, or longer than its header can state); the
    // C# library broke the connection on those, so the caller should disconnect.
    std::vector<std::uint8_t> Encrypt(const std::uint8_t* packet, std::size_t size);

private:
    std::uint8_t m_counter = 0;
};

// Server-to-client: splits the received byte stream into packets and decrypts C3/C4 ones
// (OpenMU's PipelinedSimpleModulusDecryptor with its DefaultClientKey, the counterpart of the
// server's default key). Keeps the counter.
// A decrypted C3/C4 packet keeps its C3/C4 first byte; its length field holds the decrypted length,
// so it's byte for byte the packet the server encrypted.
class MuPacketDecryptor
{
public:
    // decrypt: false for the connect server, whose packets are only split.
    explicit MuPacketDecryptor(bool decrypt = true);

    // Appends received bytes; every complete packet is passed to onPacket exactly as the C#
    // library would hand it to the native callback. Returns false on a malformed stream
    // (the caller then disconnects); after that it keeps returning false.
    // onPacket must not call Feed or destroy this decryptor: the packet points into its buffers.
    bool Feed(const std::uint8_t* data, std::size_t size,
              const std::function<void(const std::uint8_t* packet, std::size_t size)>& onPacket);

private:
    using PacketHandler = std::function<void(const std::uint8_t* packet, std::size_t size)>;

    bool HandlePacket(const std::uint8_t* packet, std::size_t size, const PacketHandler& onPacket);
    bool DecryptPacket(const std::uint8_t* packet, std::size_t size, std::size_t& decryptedSize);
    void Deliver(const std::uint8_t* packet, std::size_t size, const PacketHandler& onPacket);

    bool m_decrypt;
    bool m_failed = false;
    std::uint8_t m_counter = 0;
    // Received bytes that don't make a complete packet yet.
    std::vector<std::uint8_t> m_received;
    // The last decrypted packet; reused, so decrypting doesn't allocate once it has grown.
    std::vector<std::uint8_t> m_decrypted;
    // OpenMU splits the decrypted stream again and reads a packet only once more than 2 bytes wait,
    // so a decrypted packet of 2 bytes (a C3 holding nothing but the counter) waits for the next one.
    std::vector<std::uint8_t> m_heldBack;
};
} // namespace mu::web
