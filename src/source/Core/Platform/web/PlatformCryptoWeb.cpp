// Web build only: the browser build has no OpenSSL, so the saved-login encryption of
// Core/Platform/PlatformCrypto.cpp isn't available. Both calls fail, and the client then keeps no
// saved credentials (GameConfig treats a failed encrypt/decrypt as "nothing stored").

#include "Core/Platform/PlatformCrypto.h"

bool mu_encrypt_blob(const void* /*input*/, std::size_t /*inputSize*/, std::vector<std::uint8_t>& output)
{
    output.clear();
    return false;
}

bool mu_decrypt_blob(const void* /*input*/, std::size_t /*inputSize*/, std::vector<std::uint8_t>& output)
{
    output.clear();
    return false;
}
