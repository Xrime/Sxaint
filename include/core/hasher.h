//
// Created by xint2 on 03/07/2026.
//

#ifndef SXAINT_HASHER_H
#define SXAINT_HASHER_H
#include <cstdint>
#include <span>
#include <cstddef>
namespace sxaint::core {
    class Hasher {
    public:
        static uint32_t crc32(std::span<const std::byte> data);
        static uint32_t combine_crc32(uint32_t crc_1, uint32_t crc_2, size_t len_2);

    private:
        static const uint32_t crc32_table[256];
    };
}
#endif //SXAINT_HASHER_H