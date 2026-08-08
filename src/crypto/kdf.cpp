#include "kdf.hpp"
#include <sodium.h>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace bip39_obfuscator {
namespace crypto {

static secure_mem::byte_buffer derive_key_v1(std::string_view secret, size_t length) {
    secure_mem::byte_buffer result(length);
    size_t out_pos = 0;
    uint32_t counter = 0;

    while (out_pos < length) {
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);
        
        crypto_hash_sha256_update(&state, reinterpret_cast<const uint8_t*>(secret.data()), secret.size());

        uint8_t counter_bytes[4];
        counter_bytes[0] = static_cast<uint8_t>(counter >> 24);
        counter_bytes[1] = static_cast<uint8_t>(counter >> 16);
        counter_bytes[2] = static_cast<uint8_t>(counter >> 8);
        counter_bytes[3] = static_cast<uint8_t>(counter);

        crypto_hash_sha256_update(&state, counter_bytes, sizeof(counter_bytes));

        uint8_t hash[crypto_hash_sha256_BYTES];
        crypto_hash_sha256_final(&state, hash);

        size_t copy_len = std::min(static_cast<size_t>(crypto_hash_sha256_BYTES), length - out_pos);
        std::memcpy(result.data() + out_pos, hash, copy_len);
        out_pos += copy_len;
        
        sodium_memzero(hash, sizeof(hash));
        counter++;
    }

    return result;
}

static secure_mem::byte_buffer derive_key_v2(std::string_view secret, const std::vector<uint8_t>& salt, size_t length) {
    if (salt.size() != SALT_BYTES) {
        throw std::invalid_argument("V2 salt must be exactly 16 bytes.");
    }
    
    secure_mem::byte_buffer result(length);
    
    // N=32768, r=8, p=1
    int ret = crypto_pwhash_scryptsalsa208sha256_ll(
        reinterpret_cast<const uint8_t*>(secret.data()), secret.size(),
        salt.data(), salt.size(),
        32768, 8, 1,
        result.data(), result.size()
    );
    
    if (ret != 0) {
        throw std::runtime_error("scrypt failed");
    }
    
    return result;
}

secure_mem::byte_buffer derive_key(const KdfParams& params) {
    if (params.version == KdfVersion::V1_SHA256) {
        return derive_key_v1(params.secret, params.length);
    } else if (params.version == KdfVersion::V2_SCRYPT) {
        return derive_key_v2(params.secret, params.salt, params.length);
    }
    throw std::invalid_argument("Unknown KDF version");
}

std::vector<uint8_t> generate_salt() {
    std::vector<uint8_t> salt(SALT_BYTES);
    randombytes_buf(salt.data(), salt.size());
    return salt;
}

} // namespace crypto
} // namespace bip39_obfuscator
