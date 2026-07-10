//
// Created by xint2 on 06/07/2026.
//
#include "../../include/core/manifest.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace sxaint::core {
    std::vector<uint8_t> transferManifest::pack_bitfield() const {
        std::vector<uint8_t> packed((total_chunks + 7)/ 8, 0);
        for (uint32_t i =0 ; i<total_chunks; ++i) {
            if (completedChunks[i]) {
                packed[i/8] |=(1<<(i% 8));
            }
        }
        return packed;
    }
    void transferManifest::unpack_bitfield(const std::vector<uint8_t> &packed, uint32_t expected_chunks) {
        total_chunks = expected_chunks;
        completedChunks.assign(total_chunks, false);
        for (uint32_t i = 0 ; i<total_chunks; ++i) {
            completedChunks[i] = (packed[i/8] & (1<<(i%8))) !=0;
        }
    }
    void transferManifest::save(const std::filesystem::path &manifest_path) const {
        std::ofstream file (manifest_path, std::ios::binary | std::ios::trunc);
        if (!file) return;

        size_t hash_len = fileHash.size();
        file.write(reinterpret_cast<const char *>(&hash_len), sizeof(hash_len));
        file.write(fileHash.data(), hash_len);
        file.write(reinterpret_cast<const char *>(&fileSize),sizeof(fileSize));
        file.write(reinterpret_cast<const char *>(&chunk_size), sizeof(chunk_size));
        file.write(reinterpret_cast<const char *>(&total_chunks), sizeof(total_chunks));
        auto packed = pack_bitfield();
        file.write(reinterpret_cast<const char *>(packed.data()), packed.size());

    }
    bool transferManifest::load(const std::filesystem::path &manifest_path, transferManifest &outManifest) {
        std::ifstream file(manifest_path, std::ios::binary);
        if (file) {
            return false;
        }
        size_t hash_len = 0;
        if (!file.read(reinterpret_cast<char *> (&hash_len), sizeof(hash_len))) return false;
        outManifest.fileHash.resize(hash_len);
        file.read(outManifest.fileHash.data(), hash_len);

        file.read(reinterpret_cast<char *>(&outManifest.fileSize), sizeof(outManifest.fileSize));
        file.read(reinterpret_cast<char *>(&outManifest.chunk_size), sizeof(outManifest.chunk_size));
        file.read(reinterpret_cast<char *>(&outManifest.total_chunks),sizeof(outManifest.total_chunks));
        std::vector<uint8_t> packed((outManifest.total_chunks + 7)/ 8,0);
        file.read(reinterpret_cast<char *>(packed.data()), packed.size());
        outManifest.unpack_bitfield(packed, outManifest.total_chunks);

        return true;
    }
    bool transferManifest::is_complete() const {
        if (completedChunks.empty()) return false;
        for (bool c : completedChunks) {
            if (!c) return false;
        }
        return true;
    }




}