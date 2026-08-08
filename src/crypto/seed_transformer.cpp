#include "seed_transformer.hpp"
#include <sodium.h>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <algorithm>

namespace bip39_obfuscator {
namespace crypto {

static int find_word_index(const std::string& word, const bip39::wordlist& wl) {
    for (size_t i = 0; i < wl.size(); ++i) {
        if (wl.word(i) == word) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

secure_mem::secure_string transform_seed(const TransformParams& params, const bip39::wordlist& wl) {
    std::vector<std::string> words;
    std::stringstream ss(std::string{params.seed_phrase});
    std::string item;
    while (std::getline(ss, item, ' ')) {
        if (!item.empty()) {
            words.push_back(item);
        }
    }

    if (words.size() != 12 && words.size() != 15 && words.size() != 18 && words.size() != 21 && words.size() != 24) {
        throw std::invalid_argument("Invalid mnemonic length. Must be 12, 15, 18, 21, or 24 words.");
    }

    size_t total_bits = words.size() * 11;
    size_t ent_bits = (total_bits * 32) / 33;
    size_t cs_bits = total_bits - ent_bits;
    size_t ent_bytes = ent_bits / 8;

    // Build the bitstream
    std::vector<uint8_t> bitstream((total_bits + 7) / 8, 0);
    for (size_t i = 0; i < words.size(); ++i) {
        int idx = find_word_index(words[i], wl);
        if (idx < 0) {
            throw std::invalid_argument("Invalid word in mnemonic: " + words[i]);
        }
        for (size_t j = 0; j < 11; ++j) {
            size_t bit_pos = i * 11 + j;
            if (idx & (1 << (10 - j))) {
                bitstream[bit_pos / 8] |= (0x80 >> (bit_pos % 8));
            }
        }
    }

    // Extract entropy bytes
    secure_mem::byte_buffer entropy(ent_bytes);
    for (size_t i = 0; i < ent_bits; ++i) {
        if (bitstream[i / 8] & (0x80 >> (i % 8))) {
            entropy.data()[i / 8] |= (0x80 >> (i % 8));
        }
    }

    // Derive the key
    KdfParams kdf_params;
    kdf_params.version = params.kdf_version;
    kdf_params.secret = params.secret;
    kdf_params.salt = params.salt;
    kdf_params.length = ent_bytes;
    secure_mem::byte_buffer key = derive_key(kdf_params);

    // XOR entropy and key to create new entropy
    secure_mem::byte_buffer new_entropy(ent_bytes);
    for (size_t i = 0; i < ent_bytes; ++i) {
        new_entropy.data()[i] = entropy.data()[i] ^ key.data()[i];
    }

    // Recalculate checksum over new entropy
    uint8_t hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, new_entropy.data(), ent_bytes);

    // Build new bitstream (new_entropy + new_checksum)
    std::vector<uint8_t> new_bitstream((total_bits + 7) / 8, 0);
    for (size_t i = 0; i < ent_bits; ++i) {
        if (new_entropy.data()[i / 8] & (0x80 >> (i % 8))) {
            new_bitstream[i / 8] |= (0x80 >> (i % 8));
        }
    }
    for (size_t i = 0; i < cs_bits; ++i) {
        if (hash[0] & (0x80 >> i)) {
            size_t bit_pos = ent_bits + i;
            new_bitstream[bit_pos / 8] |= (0x80 >> (bit_pos % 8));
        }
    }
    sodium_memzero(hash, sizeof(hash));

    // Reconstruct mnemonic
    secure_mem::secure_string result_mnemonic;
    for (size_t i = 0; i < words.size(); ++i) {
        uint32_t idx = 0;
        for (size_t j = 0; j < 11; ++j) {
            size_t bit_pos = i * 11 + j;
            if (new_bitstream[bit_pos / 8] & (0x80 >> (bit_pos % 8))) {
                idx |= (1 << (10 - j));
            }
        }
        if (i > 0) result_mnemonic.append(" ", 1);
        const std::string& w = wl.word(idx);
        result_mnemonic.append(w.data(), w.size());
    }

    return result_mnemonic;
}

} // namespace crypto
} // namespace bip39_obfuscator
