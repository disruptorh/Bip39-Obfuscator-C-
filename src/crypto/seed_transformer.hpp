#pragma once

#include <string>
#include <string_view>
#include "crypto/kdf.hpp"
#include "bip39/wordlist.hpp"
#include "secure_mem/secure_buffer.hpp"

namespace bip39_obfuscator {
namespace crypto {

struct TransformParams {
    std::string_view seed_phrase;
    std::string_view secret;
    std::vector<uint8_t> salt;
    KdfVersion kdf_version;
};

/**
 * Obfuscates or de-obfuscates a BIP-39 seed phrase using XOR with a derived key.
 * Throws std::invalid_argument if the input phrase is invalid.
 */
secure_mem::secure_string transform_seed(const TransformParams& params, const bip39::wordlist& wl);

} // namespace crypto
} // namespace bip39_obfuscator
