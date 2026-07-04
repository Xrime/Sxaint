//
// Created by xint2 on 04/07/2026.
//

#ifndef SXAINT_TEST_H
#define SXAINT_TEST_H
#include <cstdint>
#include <span>
#include <vector>
namespace sxaint::core {
    struct Chunk {
        uint32_t id;
        uint32_t totalChunks;
        uint32_t payloadSize;
        uint32_t crc32;
        std::span<const std::byte> data;
        uint8_t flags{0};
    };
    class Chunker {
    public:
        static constexpr size_t kDefaultChunkSize = 2* 1024*1024;
        static std::vector<Chunk> slice(std::span<const std::byte> file_view, size_t chunk_size = kDefaultChunkSize);

    };
}
#endif //SXAINT_TEST_H
