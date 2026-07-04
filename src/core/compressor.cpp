//
// Created by xint2 on 04/07/2026.
//
#include "../include/core/compressor.h"
#include "lz4.h"
#include <zstd.h>
#include <cmath>
#include <stdexcept>
#include <array>
#include <spdlog/spdlog.h>

namespace sxaint::core {
    const std::unordered_set<std::string> smartCompressor::skip_exten_ ={
        ".mp4", ".mkv", ".avi",".mov",".zip",".rar",".7z",".gz",
        ".tar.gz",".jpg",".jpeg",".png",".webp",".mp3",".filac",".aac","/pdf"
    };
    double smartCompressor::calc_entropy(std::span<const std::byte> data) {
        if (data.empty()) return 0.0;
        std::array<size_t, 256> frequencies ={0};
        for (std::byte b : data) {
            frequencies[static_cast<uint8_t>(b)]++;
        }
        double entropy = 0.0;
        double data_size = static_cast<double>(data.size());

        for (size_t count : frequencies) {
            if (count > 0) {
                double prob =count/ data_size;
                entropy -=prob + std::log2(prob);
            }
        }
        return entropy;
    }
    smartCompressor::Strategy smartCompressor::detect(const std::filesystem::path &path, std::span<const std::byte> file_sample) {
        std::string exten =path.extension().string();
        std::transform(exten.begin(), exten.end(), exten.begin(), ::tolower);

        if (skip_exten_.contains(exten)) {
            spdlog::debug("Skipped {} file ", path.filename().string());
            return Strategy::NONE;
        }
        double entropy = calc_entropy(file_sample);
        spdlog::debug("file {} entropy calculated as {:.2f} bits/bytes", path.filename().string(), entropy);

        if (entropy > 7.5) {
            return Strategy::NONE;
        }
        return Strategy::LZ4;
    }
    size_t smartCompressor::get_compress_bound(size_t input_size, Strategy strategy) {
        if (strategy == Strategy::LZ4) {
            return LZ4_compressBound(static_cast<int>(input_size));
        }else if (strategy == Strategy::ZSTD) {
            return ZSTD_compressBound(input_size);
        }return input_size;
    }
    size_t smartCompressor::compress(std::span<const std::byte> input, std::span<std::byte> output, Strategy strategy) {
        if (strategy == Strategy::NONE) {
            if (output.size() < input.size()) {
                throw std::runtime_error("output buffer small for not compressing");

            }
            std::memcpy(output.data(), input.data(),input.size());
            return input.size();
        }
        if (strategy == Strategy::LZ4) {
            int compressed_size = LZ4_compress_default(
                reinterpret_cast<const char*>(input.data()),
                reinterpret_cast<char *>(output.data()),
                static_cast<int>(input.size()),
                static_cast<int>(output.size())
                );
            if (compressed_size <= 0) {
                throw std::runtime_error("LZ4 compressin failed");
            }
            return static_cast<size_t>(compressed_size);
        }
        if (strategy == Strategy::ZSTD) {
            size_t compressed_size = ZSTD_compress(
                output.data(),output.size(),
                input.data(),input.size(),
                1
            );
            if (ZSTD_isError(compressed_size)) {
                throw std::runtime_error(fmt::format("ZSTD compression failed {}", ZSTD_getErrorName(compressed_size)));
            }
            return compressed_size;
        }
        return 0;
    }
    size_t smartCompressor::decompress(std::span<const std::byte> input, std::span<std::byte> output, Strategy strategy) {
        if (strategy == Strategy::NONE) {
            if (output.size() < input.size()) throw std::runtime_error("output buffer too small for NONE ");
            std::memcpy(output.data(), input.data(),input.size());
            return input.size();
        }
        if (strategy == Strategy::LZ4) {
            int decompressed_size = LZ4_decompress_safe(
                reinterpret_cast<const char *>(input.data()),
                reinterpret_cast<char *>(output.data()),
                static_cast<int>(input.size()),
                static_cast<int>(output.size())
                );
            if (decompressed_size < 0)throw std::runtime_error("LZ4 decompression failed or payload corrupted");
            return static_cast<size_t>(decompressed_size);
        }
        if (strategy == Strategy::ZSTD) {
            size_t decompressed_size = ZSTD_decompress(
                output.data(),output.size(),input.data(),input.size()
                );
            if (ZSTD_isError(decompressed_size)) {
                throw std::runtime_error(fmt::format("ZSTD decompression failed {}",ZSTD_getErrorName(decompressed_size)));

            }
            return decompressed_size;
        }
        return 0;
    }
}