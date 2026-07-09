//
// Created by xint2 on 09/07/2026.
//

#ifndef SXAINT_CRYPTO_H
#define SXAINT_CRYPTO_H
#include <span>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace sxaint::core {
    class Crypto {
    public:
        static std::vector<std::byte> derive_key(uint32_t pin);
        static std::vector<std::byte> encrypt(
            std::span<const std::byte> plaintext,
            const std::vector<std::byte>& key,
            uint32_t chunk_id
            );
        static std::vector<std::byte> decrypt(
            std::span<const std::byte> ciphertext,
            const std::vector<std::byte>& key,
            uint32_t chunk_id
            );
    };
}
#endif //SXAINT_CRYPTO_H