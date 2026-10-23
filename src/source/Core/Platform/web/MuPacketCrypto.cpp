// Web build only: see MuPacketCrypto.h.
//
// A port of OpenMU's MUnique.OpenMU.Network (Xor/PipelinedXor32Encryptor.cs and
// SimpleModulus/PipelinedSimpleModulus*.cs), kept close to the C# so the two stay comparable.
// Packets: C1/C3 have a 2 byte header (type, length), C2/C4 a 3 byte one (type, length big endian).
//
// SimpleModulus works on blocks: 8 plain bytes become 11 encrypted ones. The 8 bytes are read as four
// 16 bit values; each is encrypted to an 18 bit value, and the four are packed into 9 bytes, followed by
// the block's length and a checksum. Before the content of a C3/C4 packet goes in a 1 byte counter,
// which counts the packets of the connection and lets the receiver reject replayed ones.

#include "Core/Platform/web/MuPacketCrypto.h"

#include <algorithm>
#include <array>

namespace
{
constexpr std::uint8_t C1Packet = 0xC1;
constexpr std::uint8_t C2Packet = 0xC2;
constexpr std::uint8_t C3Packet = 0xC3;
constexpr std::uint8_t C4Packet = 0xC4;
constexpr std::size_t ShortHeaderSize = 2;
constexpr std::size_t LongHeaderSize = 3;
constexpr std::size_t MaxShortPacketSize = 0xFF;
constexpr std::size_t MaxLongPacketSize = 0xFFFF;

// OpenMU reads 3 bytes of a packet before it looks at it: its splitter waits until more than 2 bytes
// are buffered, and both pipelined crypto classes copy a 3 byte header first.
constexpr std::size_t MinPacketBytes = 3;

constexpr std::size_t DecryptedBlockSize = 8;
constexpr std::size_t EncryptedBlockSize = 11;
constexpr std::size_t ValuesPerBlock = 4;
constexpr int BitsPerByte = 8;
constexpr int BitsPerValue = 18;
constexpr std::uint32_t LowWordMask = 0xFFFF;
constexpr std::uint32_t ByteMask = 0xFF;
constexpr std::size_t BlockLengthOffset = EncryptedBlockSize - 2;
constexpr std::size_t BlockChecksumOffset = EncryptedBlockSize - 1;
constexpr std::uint8_t BlockSizeXorKey = 0x3D;
constexpr std::uint8_t BlockChecksumXorKey = 0xF8;

// OpenMU's SimpleModulusKeys; crypt holds the multipliers of the direction (EncryptKey or DecryptKey).
struct SimpleModulusKeys
{
    std::array<std::uint32_t, ValuesPerBlock> modulus;
    std::array<std::uint32_t, ValuesPerBlock> crypt;
    std::array<std::uint32_t, ValuesPerBlock> xorKey;
};

// PipelinedSimpleModulusEncryptor.DefaultClientKey: the client encrypts with it.
constexpr SimpleModulusKeys ClientEncryptionKeys{
    {128079, 164742, 70235, 106898}, {23489, 11911, 19816, 13647}, {48413, 46165, 15171, 37433}};

// PipelinedSimpleModulusDecryptor.DefaultClientKey: decrypts what the server encrypts with its default key.
constexpr SimpleModulusKeys ClientDecryptionKeys{
    {73326, 109989, 98843, 171058}, {18035, 30340, 24701, 11141}, {62004, 64409, 35374, 64599}};

// Xor.DefaultKeys.Xor32Key.
constexpr std::array<std::uint8_t, 32> Xor32Key{
    0xAB, 0x11, 0xCD, 0xFE, 0x18, 0x23, 0xC5, 0xA3, 0xCA, 0x33, 0xC1, 0xCC, 0x66, 0x67, 0x21, 0xF3,
    0x32, 0x12, 0x15, 0x35, 0x29, 0xFF, 0xFE, 0x1D, 0x44, 0xEF, 0xCD, 0x41, 0x26, 0x3C, 0x4E, 0x4D,
};

using BlockValues = std::array<std::uint32_t, ValuesPerBlock>;

// 0 for a first byte that isn't C1-C4.
std::size_t GetHeaderSize(std::uint8_t packetType)
{
    switch (packetType)
    {
    case C1Packet:
    case C3Packet:
        return ShortHeaderSize;
    case C2Packet:
    case C4Packet:
        return LongHeaderSize;
    default:
        return 0;
    }
}

// The length a packet's header states; 0 for a first byte that isn't C1-C4. Needs 3 bytes.
std::size_t GetPacketSize(const std::uint8_t* packet)
{
    switch (GetHeaderSize(packet[0]))
    {
    case ShortHeaderSize:
        return packet[1];
    case LongHeaderSize:
        return static_cast<std::size_t>(packet[1]) << BitsPerByte | packet[2];
    default:
        return 0;
    }
}

// Writes size into the length field of a C1-C4 packet. Like OpenMU, it keeps only the bits that fit.
void SetPacketSize(std::uint8_t* packet, std::size_t size)
{
    if (GetHeaderSize(packet[0]) == ShortHeaderSize)
    {
        packet[1] = static_cast<std::uint8_t>(size);
        return;
    }
    packet[1] = static_cast<std::uint8_t>(size >> BitsPerByte);
    packet[2] = static_cast<std::uint8_t>(size);
}

std::uint32_t ReverseBytes(std::uint32_t value)
{
    return (value >> 24) | ((value >> 8) & 0xFF00) | ((value << 8) & 0xFF0000) | (value << 24);
}

// The block's bytes as 16 bit values, little endian (OpenMU casts the byte span to ushort).
std::uint32_t ReadWord(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << BitsPerByte;
}

void WriteWord(std::uint8_t* bytes, std::uint32_t value)
{
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> BitsPerByte);
}

// The bits, in the last byte of a packed value, that hold its 2 highest bits.
std::uint32_t GetRemainderMask(int bitOffset)
{
    const std::uint32_t upTo = (ByteMask << (6 - bitOffset)) & ByteMask;
    const std::uint32_t before = (ByteMask << (BitsPerByte - bitOffset)) & ByteMask;
    return upTo - before;
}

// PipelinedSimpleModulusEncryptor.WriteResultToTarget: packs an 18 bit value into the block.
void WriteValue(std::uint8_t* block, int index, std::uint32_t value)
{
    const int bitIndex = index * BitsPerValue;
    std::size_t byteOffset = static_cast<std::size_t>(bitIndex / BitsPerByte);
    const int bitOffset = bitIndex % BitsPerByte;
    const std::uint32_t firstMask = ByteMask >> bitOffset;
    const std::uint32_t swapped = ReverseBytes(value);
    block[byteOffset++] |= static_cast<std::uint8_t>((swapped >> (24 + bitOffset)) & firstMask);
    block[byteOffset++] = static_cast<std::uint8_t>(swapped >> (16 + bitOffset));
    block[byteOffset] = static_cast<std::uint8_t>((swapped >> (8 + bitOffset)) & (ByteMask << (8 - bitOffset)));
    const std::uint32_t remainder = (value >> 16) << (6 - bitOffset);
    block[byteOffset] |= static_cast<std::uint8_t>(remainder & GetRemainderMask(bitOffset));
}

// PipelinedSimpleModulusDecryptor.ReadInputBuffer: unpacks an 18 bit value from the block.
std::uint32_t ReadValue(const std::uint8_t* block, int index)
{
    const int bitIndex = index * BitsPerValue;
    std::size_t byteOffset = static_cast<std::size_t>(bitIndex / BitsPerByte);
    const int bitOffset = bitIndex % BitsPerByte;
    const std::uint32_t firstMask = ByteMask >> bitOffset;
    std::uint32_t result = 0;
    result += (block[byteOffset++] & firstMask) << (24 + bitOffset);
    result += static_cast<std::uint32_t>(block[byteOffset++]) << (16 + bitOffset);
    result += (block[byteOffset] & (ByteMask << (8 - bitOffset))) << (8 + bitOffset);
    result = ReverseBytes(result);
    const std::uint32_t remainder = block[byteOffset] & GetRemainderMask(bitOffset);
    result += (remainder << 16) >> (6 - bitOffset);
    return result;
}

// PipelinedSimpleModulusEncryptor.EncryptContent. The arithmetic is 32 bit unsigned and wraps, as in C#.
BlockValues EncryptValues(const std::uint8_t* plain, const SimpleModulusKeys& keys)
{
    BlockValues values{};
    values[0] = ((keys.xorKey[0] ^ ReadWord(plain)) * keys.crypt[0]) % keys.modulus[0];
    for (std::size_t i = 1; i < ValuesPerBlock; ++i)
    {
        const std::uint32_t input = ReadWord(plain + i * 2) ^ (values[i - 1] & LowWordMask);
        values[i] = ((keys.xorKey[i] ^ input) * keys.crypt[i]) % keys.modulus[i];
    }

    for (std::size_t i = 0; i + 1 < ValuesPerBlock; ++i)
    {
        values[i] = values[i] ^ keys.xorKey[i] ^ (values[i + 1] & LowWordMask);
    }
    return values;
}

// PipelinedSimpleModulusDecryptor.DecryptContent: writes the 8 plain bytes of the block.
void DecryptValues(BlockValues values, const SimpleModulusKeys& keys, std::uint8_t* plain)
{
    for (std::size_t i = ValuesPerBlock - 1; i > 0; --i)
    {
        values[i - 1] = values[i - 1] ^ keys.xorKey[i - 1] ^ (values[i] & LowWordMask);
    }

    for (std::size_t i = 0; i < ValuesPerBlock; ++i)
    {
        std::uint32_t result = keys.xorKey[i] ^ ((values[i] * keys.crypt[i]) % keys.modulus[i]);
        if (i > 0)
        {
            result ^= values[i - 1] & LowWordMask;
        }
        WriteWord(plain + i * 2, result);
    }
}

// Encrypts one zero-padded block of 8 bytes, of which blockSize are content, into 11 bytes.
void EncryptBlock(const std::array<std::uint8_t, DecryptedBlockSize>& plain, std::size_t blockSize,
                  std::uint8_t* encrypted)
{
    std::fill(encrypted, encrypted + EncryptedBlockSize, std::uint8_t{0});
    const BlockValues values = EncryptValues(plain.data(), ClientEncryptionKeys);
    for (std::size_t i = 0; i < ValuesPerBlock; ++i)
    {
        WriteValue(encrypted, static_cast<int>(i), values[i]);
    }

    std::uint8_t checksum = BlockChecksumXorKey;
    for (std::size_t i = 0; i < blockSize; ++i)
    {
        checksum ^= plain[i];
    }
    encrypted[BlockLengthOffset] = static_cast<std::uint8_t>(blockSize ^ BlockSizeXorKey ^ checksum);
    encrypted[BlockChecksumOffset] = checksum;
}

// Decrypts 11 bytes into 8 and returns how many of them are content; false when the block is broken
// (OpenMU throws InvalidBlockSizeException or InvalidBlockChecksumException).
bool DecryptBlock(const std::uint8_t* encrypted, std::uint8_t* plain, std::size_t& blockSize)
{
    BlockValues values{};
    for (std::size_t i = 0; i < ValuesPerBlock; ++i)
    {
        values[i] = ReadValue(encrypted, static_cast<int>(i));
    }
    DecryptValues(values, ClientDecryptionKeys, plain);

    const std::uint8_t checksumOnWire = encrypted[BlockChecksumOffset];
    blockSize = static_cast<std::uint8_t>(encrypted[BlockLengthOffset] ^ checksumOnWire ^ BlockSizeXorKey);
    if (blockSize > DecryptedBlockSize)
    {
        return false;
    }

    std::uint8_t checksum = BlockChecksumXorKey;
    for (std::size_t i = 0; i < DecryptedBlockSize; ++i)
    {
        checksum ^= plain[i];
    }
    return checksum == checksumOnWire;
}

// PipelinedXor32Encryptor: each content byte after the first is xored with the previous, already
// encrypted byte and the key byte of its position in the packet.
void ApplyXor32(std::uint8_t* packet, std::size_t size)
{
    const std::size_t headerSize = GetHeaderSize(packet[0]);
    for (std::size_t i = headerSize + 1; i < size; ++i)
    {
        packet[i] = static_cast<std::uint8_t>(packet[i] ^ packet[i - 1] ^ Xor32Key[i % Xor32Key.size()]);
    }
}

// PipelinedSimpleModulusEncryptor.EncryptAndWrite for a C3/C4 packet: the counter and the content,
// in blocks of 8, behind the plain header with the encrypted length.
std::vector<std::uint8_t> EncryptSimpleModulus(const std::vector<std::uint8_t>& plain, std::uint8_t counter)
{
    const std::size_t headerSize = GetHeaderSize(plain[0]);
    const std::uint8_t* content = plain.data() + headerSize;
    const std::size_t contentSize = plain.size() - headerSize;
    const std::size_t countedSize = contentSize + 1;
    const std::size_t blockCount = (countedSize + DecryptedBlockSize - 1) / DecryptedBlockSize;

    std::vector<std::uint8_t> encrypted(headerSize + blockCount * EncryptedBlockSize);
    encrypted[0] = plain[0];
    // A C3 over 185 or a C4 over 47,658 plain bytes no longer fits its length field; OpenMU then keeps
    // only the low bits, and so does this.
    SetPacketSize(encrypted.data(), encrypted.size());

    std::array<std::uint8_t, DecryptedBlockSize> block{};
    for (std::size_t offset = 0; offset < countedSize; offset += DecryptedBlockSize)
    {
        const std::size_t blockSize = std::min(DecryptedBlockSize, countedSize - offset);
        block.fill(0);
        for (std::size_t i = 0; i < blockSize; ++i)
        {
            const std::size_t countedIndex = offset + i;
            block[i] = countedIndex == 0 ? counter : content[countedIndex - 1];
        }
        const std::size_t blockIndex = offset / DecryptedBlockSize;
        EncryptBlock(block, blockSize, encrypted.data() + headerSize + blockIndex * EncryptedBlockSize);
    }
    return encrypted;
}

// What ConnectionManager_Send and OpenMU's pipes can carry.
bool CanSend(const std::uint8_t* packet, std::size_t size)
{
    if (packet == nullptr || size < MinPacketBytes)
    {
        return false;
    }

    switch (GetHeaderSize(packet[0]))
    {
    case ShortHeaderSize:
        return size <= MaxShortPacketSize;
    case LongHeaderSize:
        return size <= MaxLongPacketSize;
    default:
        return false;
    }
}
} // namespace

namespace mu::web
{
std::vector<std::uint8_t> MuPacketEncryptor::Encrypt(const std::uint8_t* packet, std::size_t size)
{
    if (!CanSend(packet, size))
    {
        return {};
    }

    std::vector<std::uint8_t> plain(packet, packet + size);
    SetPacketSize(plain.data(), size);
    ApplyXor32(plain.data(), plain.size());
    if (plain[0] < C3Packet)
    {
        return plain;
    }

    std::vector<std::uint8_t> encrypted = EncryptSimpleModulus(plain, m_counter);
    ++m_counter;
    return encrypted;
}

MuPacketDecryptor::MuPacketDecryptor(bool decrypt) : m_decrypt(decrypt)
{
}

bool MuPacketDecryptor::Feed(const std::uint8_t* data, std::size_t size,
                             const std::function<void(const std::uint8_t* packet, std::size_t size)>& onPacket)
{
    if (m_failed)
    {
        return false;
    }

    m_received.insert(m_received.end(), data, data + size);
    std::size_t offset = 0;
    bool ok = true;
    // Like OpenMU's PacketPipeReaderBase: a header is read only once more than 2 bytes are buffered.
    while (ok && m_received.size() - offset >= MinPacketBytes)
    {
        const std::uint8_t* packet = m_received.data() + offset;
        const std::size_t packetSize = GetPacketSize(packet);
        if (packetSize == 0)
        {
            ok = false;
            break;
        }
        if (m_received.size() - offset < packetSize)
        {
            break;
        }

        ok = HandlePacket(packet, packetSize, onPacket);
        offset += packetSize;
    }

    if (!ok)
    {
        m_failed = true;
        m_received.clear();
        return false;
    }
    m_received.erase(m_received.begin(), m_received.begin() + static_cast<std::ptrdiff_t>(offset));
    return true;
}

bool MuPacketDecryptor::HandlePacket(const std::uint8_t* packet, std::size_t size, const PacketHandler& onPacket)
{
    if (!m_decrypt)
    {
        onPacket(packet, size);
        return true;
    }

    // OpenMU's decryptor copies a 3 byte header before it looks at the packet.
    if (size < MinPacketBytes)
    {
        return false;
    }

    if (packet[0] < C3Packet)
    {
        Deliver(packet, size, onPacket);
        return true;
    }

    std::size_t decryptedSize = 0;
    if (!DecryptPacket(packet, size, decryptedSize))
    {
        return false;
    }
    Deliver(m_decrypted.data(), decryptedSize, onPacket);
    return true;
}

// PipelinedSimpleModulusDecryptor.DecryptAndWrite, into m_decrypted.
bool MuPacketDecryptor::DecryptPacket(const std::uint8_t* packet, std::size_t size, std::size_t& decryptedSize)
{
    const std::size_t headerSize = GetHeaderSize(packet[0]);
    const std::size_t encryptedSize = size - headerSize;
    if (encryptedSize == 0 || encryptedSize % EncryptedBlockSize != 0)
    {
        return false;
    }

    // As in OpenMU, the blocks are decrypted from the header's last byte on: the counter lands there
    // and the length overwrites it afterwards.
    const std::size_t blockCount = encryptedSize / EncryptedBlockSize;
    m_decrypted.resize(headerSize - 1 + blockCount * DecryptedBlockSize);
    std::uint8_t* output = m_decrypted.data() + headerSize - 1;
    std::size_t contentSize = 0;
    for (std::size_t block = 0; block < blockCount; ++block)
    {
        std::size_t blockSize = 0;
        const std::uint8_t* encrypted = packet + headerSize + block * EncryptedBlockSize;
        if (!DecryptBlock(encrypted, output + contentSize, blockSize))
        {
            return false;
        }
        // OpenMU checks the counter while nothing is decrypted yet, so also after a block of length 0.
        if (contentSize == 0 && output[0] != m_counter)
        {
            return false;
        }
        // Each block goes right behind the content so far, even when a block before it was short.
        contentSize += blockSize;
    }
    ++m_counter;

    decryptedSize = headerSize - 1 + contentSize;
    // OpenMU fails to write the length into a packet shorter than its header.
    if (decryptedSize < headerSize)
    {
        return false;
    }
    m_decrypted[0] = packet[0];
    SetPacketSize(m_decrypted.data(), decryptedSize);
    return true;
}

// The second split of OpenMU's Connection, which reads the decrypted packets: it passes a packet on
// once more than 2 bytes wait, so a 2 byte packet waits for the next one.
void MuPacketDecryptor::Deliver(const std::uint8_t* packet, std::size_t size, const PacketHandler& onPacket)
{
    if (!m_heldBack.empty())
    {
        onPacket(m_heldBack.data(), m_heldBack.size());
        m_heldBack.clear();
    }

    if (size < MinPacketBytes)
    {
        m_heldBack.assign(packet, packet + size);
        return;
    }
    onPacket(packet, size);
}
} // namespace mu::web
