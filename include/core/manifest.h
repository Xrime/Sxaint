//
// Created by xint2 on 06/07/2026.
//

#ifndef SXAINT_MANIFEST_H
#define SXAINT_MANIFEST_H
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>

namespace sxaint::core {
    struct transferManifest {
        std::string fileHash;
        uint64_t fileSize=0;
        uint32_t chunk_size = 0;
        uint32_t total_chunks = 0;
        std::vector<bool> completedChunks;
        void save(const std::filesystem::path& manifest_path ) const;
        static bool load(const std::filesystem::path& manifest_path, transferManifest& outManifest);
        bool is_complete() const;
        std::vector<uint8_t> pack_bitfield() const;
        void unpack_bitfield(const std::vector<uint8_t>& packed, uint32_t expected_chunks);
    };
}
#endif //SXAINT_MANIFEST_H