// Web build only: the saved login (GameConfig's remembered user name and, with the player's consent, the
// password) encrypted without OpenSSL, which the browser build doesn't have.
//
// The key is 32 random bytes this browser keeps in its own storage entry ("mu-web/login-key"), apart from
// config.ini ("mu-web/config.ini", index.html): the config text alone, copied somewhere, doesn't give the
// login away. Anything that can read this site's storage in this browser can read both, as the desktop
// client's key comes from the computer's name; it keeps the password out of plain sight, nothing more.
//
// Cipher: SHA-256 in counter mode (block i = HMAC(encKey, nonce | i)), then a 16-byte HMAC-SHA256 tag over
// version, nonce and ciphertext; encKey and macKey are HMACs of the stored key. Same layout as the desktop's
// AES-GCM blob: version byte, 12-byte nonce, ciphertext, 16-byte tag, with its own version byte (0x02), so
// a config.ini from the desktop client simply doesn't decrypt here (nothing stored) and the other way round.
//
// Before 2026-10-31 both calls failed and the browser kept no login at all (ghinzuro: the installed web app
// forgot the login and the password every time).

#include "Core/Platform/PlatformCrypto.h"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace
{
    constexpr std::uint8_t VersionWeb = 0x02;
    constexpr std::size_t KeySize = 32;
    constexpr std::size_t NonceSize = 12;
    constexpr std::size_t TagSize = 16;
    constexpr std::size_t Overhead = 1 + NonceSize + TagSize;
    constexpr std::size_t MaxInput = 64 * 1024;

    using Digest = std::array<std::uint8_t, 32>;

    // SHA-256 (FIPS 180-4).
    class Sha256
    {
    public:
        void Add(const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            m_length += size;
            while (size > 0)
            {
                const std::size_t take = std::min(size, sizeof(m_block) - m_used);
                std::memcpy(m_block + m_used, bytes, take);
                m_used += take;
                bytes += take;
                size -= take;
                if (m_used == sizeof(m_block))
                {
                    Compress();
                    m_used = 0;
                }
            }
        }

        Digest Finish()
        {
            const std::uint64_t bits = m_length * 8;
            const std::uint8_t one = 0x80;
            Add(&one, 1);
            const std::uint8_t zero = 0;
            while (m_used != 56)
            {
                Add(&zero, 1);
            }
            std::uint8_t length[8];
            for (int i = 0; i < 8; ++i)
            {
                length[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
            }
            Add(length, 8);
            Digest out;
            for (int i = 0; i < 8; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    out[i * 4 + j] = static_cast<std::uint8_t>(m_state[i] >> (24 - 8 * j));
                }
            }
            return out;
        }

    private:
        static std::uint32_t Rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

        void Compress()
        {
            static constexpr std::uint32_t K[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
            std::uint32_t w[64];
            for (int i = 0; i < 16; ++i)
            {
                w[i] = (std::uint32_t(m_block[i * 4]) << 24) | (std::uint32_t(m_block[i * 4 + 1]) << 16)
                       | (std::uint32_t(m_block[i * 4 + 2]) << 8) | std::uint32_t(m_block[i * 4 + 3]);
            }
            for (int i = 16; i < 64; ++i)
            {
                const std::uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                const std::uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            std::uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
            std::uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
            for (int i = 0; i < 64; ++i)
            {
                const std::uint32_t t1 = h + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
                const std::uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }
            m_state[0] += a;
            m_state[1] += b;
            m_state[2] += c;
            m_state[3] += d;
            m_state[4] += e;
            m_state[5] += f;
            m_state[6] += g;
            m_state[7] += h;
        }

        std::uint32_t m_state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::uint8_t m_block[64] = {};
        std::size_t m_used = 0;
        std::uint64_t m_length = 0;
    };

    // HMAC-SHA256 (RFC 2104) over the concatenation of up to three pieces.
    Digest Hmac(const std::uint8_t* key, std::size_t keySize, const void* a, std::size_t aSize,
                const void* b = nullptr, std::size_t bSize = 0, const void* c = nullptr, std::size_t cSize = 0)
    {
        std::uint8_t block[64] = {};
        if (keySize > sizeof(block))
        {
            Sha256 longKey;
            longKey.Add(key, keySize);
            const Digest hashed = longKey.Finish();
            std::memcpy(block, hashed.data(), hashed.size());
        }
        else
        {
            std::memcpy(block, key, keySize);
        }
        std::uint8_t pad[64];
        for (int i = 0; i < 64; ++i)
        {
            pad[i] = block[i] ^ 0x36;
        }
        Sha256 inner;
        inner.Add(pad, sizeof(pad));
        if (aSize) inner.Add(a, aSize);
        if (bSize) inner.Add(b, bSize);
        if (cSize) inner.Add(c, cSize);
        const Digest innerDigest = inner.Finish();
        for (int i = 0; i < 64; ++i)
        {
            pad[i] = block[i] ^ 0x5c;
        }
        Sha256 outer;
        outer.Add(pad, sizeof(pad));
        outer.Add(innerDigest.data(), innerDigest.size());
        std::memset(block, 0, sizeof(block));
        std::memset(pad, 0, sizeof(pad));
        return outer.Finish();
    }

    // The browser's login key as 64 hex digits, made on first use when create is set. Null without
    // storage (private windows of some browsers) or, unless create, without a key yet.
    EM_JS(char*, MuWeb_LoginKey, (int create), {
        try {
            let key = localStorage.getItem("mu-web/login-key");
            if (!key || !/^[0-9a-f]{64}$/.test(key)) {
                if (!create) {
                    return 0;
                }
                const bytes = crypto.getRandomValues(new Uint8Array(32));
                key = Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
                localStorage.setItem("mu-web/login-key", key);
                if (localStorage.getItem("mu-web/login-key") !== key) {
                    return 0;
                }
            }
            return stringToNewUTF8(key);
        } catch (e) {
            return 0;
        }
    });

    struct Keys
    {
        Digest enc;
        Digest mac;
    };

    bool GetKeys(bool create, Keys& keys)
    {
        char* hex = MuWeb_LoginKey(create ? 1 : 0);
        if (!hex)
        {
            return false;
        }
        std::uint8_t key[KeySize];
        bool ok = std::strlen(hex) == KeySize * 2;
        for (std::size_t i = 0; ok && i < KeySize; ++i)
        {
            char pair[3] = {hex[i * 2], hex[i * 2 + 1], 0};
            key[i] = static_cast<std::uint8_t>(std::strtoul(pair, nullptr, 16));
        }
        std::memset(hex, 0, std::strlen(hex));
        std::free(hex);
        if (!ok)
        {
            return false;
        }
        keys.enc = Hmac(key, KeySize, "mu-web login enc", 16);
        keys.mac = Hmac(key, KeySize, "mu-web login mac", 16);
        std::memset(key, 0, sizeof(key));
        return true;
    }

    // XORs data with the keystream for this nonce.
    void ApplyKeystream(const Keys& keys, const std::uint8_t* nonce, std::uint8_t* data, std::size_t size)
    {
        for (std::uint32_t counter = 0; size > 0; ++counter)
        {
            const std::uint8_t be[4] = {static_cast<std::uint8_t>(counter >> 24), static_cast<std::uint8_t>(counter >> 16),
                                        static_cast<std::uint8_t>(counter >> 8), static_cast<std::uint8_t>(counter)};
            const Digest block = Hmac(keys.enc.data(), keys.enc.size(), nonce, NonceSize, be, sizeof(be));
            const std::size_t take = std::min(size, block.size());
            for (std::size_t i = 0; i < take; ++i)
            {
                data[i] ^= block[i];
            }
            data += take;
            size -= take;
        }
    }

    Digest Tag(const Keys& keys, const std::uint8_t* versionAndNonce, const std::uint8_t* cipher, std::size_t size)
    {
        return Hmac(keys.mac.data(), keys.mac.size(), versionAndNonce, 1 + NonceSize, cipher, size);
    }
}

bool mu_encrypt_blob(const void* input, std::size_t inputSize, std::vector<std::uint8_t>& output)
{
    output.clear();
    Keys keys;
    if (!input || inputSize == 0 || inputSize > MaxInput || !GetKeys(true, keys))
    {
        return false;
    }
    output.resize(Overhead + inputSize);
    output[0] = VersionWeb;
    if (getentropy(output.data() + 1, NonceSize) != 0)
    {
        output.clear();
        return false;
    }
    std::uint8_t* cipher = output.data() + 1 + NonceSize;
    std::memcpy(cipher, input, inputSize);
    ApplyKeystream(keys, output.data() + 1, cipher, inputSize);
    const Digest tag = Tag(keys, output.data(), cipher, inputSize);
    std::memcpy(cipher + inputSize, tag.data(), TagSize);
    return true;
}

bool mu_decrypt_blob(const void* input, std::size_t inputSize, std::vector<std::uint8_t>& output)
{
    output.clear();
    const auto* bytes = static_cast<const std::uint8_t*>(input);
    Keys keys;
    if (!bytes || inputSize <= Overhead || inputSize > Overhead + MaxInput || bytes[0] != VersionWeb
        || !GetKeys(false, keys))
    {
        return false;
    }
    const std::size_t size = inputSize - Overhead;
    const std::uint8_t* cipher = bytes + 1 + NonceSize;
    const Digest tag = Tag(keys, bytes, cipher, size);
    std::uint8_t difference = 0;
    for (std::size_t i = 0; i < TagSize; ++i)
    {
        difference |= tag[i] ^ cipher[size + i];
    }
    if (difference != 0)
    {
        return false;
    }
    output.assign(cipher, cipher + size);
    ApplyKeystream(keys, bytes + 1, output.data(), size);
    return true;
}
