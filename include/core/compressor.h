//
// Created by xint2 on 04/07/2026.
//

#ifndef SXAINT_COMPRESSOR_H
#define SXAINT_COMPRESSOR_H

#include <span>
#include <cstddef>
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_set>
namespace sxaint::core{
    class smartCompressor {
    public:
        enum class Strategy {NONE, LZ4, ZSTD};
        static Strategy detect(const std::filesystem::path& path, std::span<const std::byte> file_sample);
        static size_t compress(std::span<const std::byte> input, std::span<std::byte> output, Strategy strategy);
        static size_t decompress(std::span<const std::byte> input, std::span<std::byte> output, Strategy strategy);
        static size_t get_compress_bound(size_t input_size, Strategy strategy);

    private:
        static double calc_entropy(std::span<const std::byte> data);
        static const std::unordered_set<std::string> skip_exten_;
    };
}
#endif //SXAINT_COMPRESSOR_H
