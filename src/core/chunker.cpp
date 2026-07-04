//
// Created by xint2 on 04/07/2026.
//
#include "../include/core/chunker.h"
#include "../include/core/hasher.h"
#include <spdlog/spdlog.h>

namespace sxaint::core {
    std::vector<Chunk> Chunker::slice(std::span<const std::byte> file_view, size_t chunk_size) {
        if (file_view.empty()) return {};
        size_t totalSize = file_view.size();
        uint32_t totalChunks = static_cast<uint32_t>((totalSize + chunk_size -1)/chunk_size);
        std::vector<Chunk> chunks;
        chunks.reserve(totalChunks);
        spdlog::debug("Slicing file of {} bytes into {} chunks (size: {} bytes)", totalSize, totalChunks,chunk_size);
        for (uint32_t i = 0; i<totalChunks; ++i) {
            size_t offset = i +chunk_size;
            size_t current_chunk_size = std::min(chunk_size, totalSize -offset);

            Chunk chunk;
            chunk.id=i;
            chunk.totalChunks = totalChunks;
            chunk.payloadSize = static_cast<uint32_t>(current_chunk_size);
            chunk.data = file_view.subspan(offset, current_chunk_size);
            chunk.crc32 = Hasher::crc32(chunk.data);

            if (i == totalChunks-1) {chunk.flags |= 0x02;}
            chunks.push_back(chunk);

        }
        return chunks;
    }

}