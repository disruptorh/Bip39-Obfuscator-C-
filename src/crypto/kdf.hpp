#pragma once

#include "secure_mem/secure_buffer.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace bip39_obfuscator {
namespace crypto {

enum class KdfVersion {
    V1_SHA256 = 1,
    V2_SCRYPT = 2
};

struct KdfParams {
    KdfVersion version;
    std::string_view secret;
    std::vector<uint8_t> salt; // Used only in V2
    size_t length;             // Target output length
};

constexpr size_t SALT_BYTES = 16;

/**
 * Derives a key using either V1 (legacy SHA-256) or V2 (scrypt).
 * Returns a secure_buffer containing the derived key.
 */
secure_mem::byte_buffer derive_key(const KdfParams& params);

/**
 * Generates a random salt for V2.
 */
std::vector<uint8_t> generate_salt();

} // namespace crypto
} // namespace bip39_obfuscator
