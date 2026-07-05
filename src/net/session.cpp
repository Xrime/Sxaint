//
// Created by xint2 on 05/07/2026.
//
#include "../include/net/session.h"
#include "../include/core/chunker.h"
#include "../include/core/compressor.h"
#include "../include/core/hasher.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace sxaint::net {

    Session::Session() : thread_pool_(0) {
        transport_.setRecvCallback([this](std::vector<std::byte>&& data) {
            this->handleRecv(std::move(data));
        });
    }

    void Session::sendFile(const std::filesystem::path &file_path, const std::string &target_ip, uint16_t port) {
        spdlog::info("Starting sender session for {}", file_path.string());

        uint64_t file_size = std::filesystem::file_size(file_path);
        auto file_view = file_mapper_.map_read(file_path, 0, file_size);

        transport_.connect(target_ip, port);

        handshake hs{};
        hs.file_size = file_size;
        hs.chunk_size = core::Chunker::kDefaultChunkSize;

        auto chunks = core::Chunker::slice(file_view, hs.chunk_size);
        hs.total_chunks = static_cast<uint32_t>(chunks.size());

        std::string filename = file_path.filename().string();
        std::strncpy(hs.file_name, filename.c_str(), sizeof(hs.file_name)-1);

        core::Chunk hs_chunk{};
        hs_chunk.payloadSize = sizeof(handshake);
        hs_chunk.data = std::span<const std::byte>(reinterpret_cast<const std::byte*>(&hs), sizeof(handshake));
        transport_.sendChunk(hs_chunk);

        auto sample_size = std::min<size_t>(4096, file_size);
        auto strategy = core::smartCompressor::detect(file_path, std::span<const std::byte>(file_view.data(), sample_size));

        std::vector<std::future<void>> futures;

        for (const auto& chunk : chunks) {
            // CRITICAL FIX: Extract primitive types so MinGW doesn't corrupt the memory inside the thread
            uint32_t c_id = chunk.id;
            uint32_t c_size = chunk.payloadSize;
            uint32_t c_crc32 = chunk.crc32;
            const std::byte* c_data_ptr = chunk.data.data();

            futures.push_back(thread_pool_.submit([this, strategy, c_id, c_size, c_crc32, c_data_ptr]() {
                try {
                    size_t max_bound = core::smartCompressor::get_compress_bound(c_size, strategy);
                    std::vector<std::byte> wireBuffer(sizeof(chunkWireHeader) + max_bound);

                    chunkWireHeader header{};
                    header.type = messageType::chunkData;
                    header.chunk_id = c_id;
                    header.original_size = c_size;
                    header.crc32 = c_crc32;
                    header.compress_stra = static_cast<uint8_t>(strategy);

                    std::span<std::byte> out_payload(wireBuffer.data() + sizeof(chunkWireHeader), max_bound);

                    // Safely reconstruct the span locally inside the thread
                    std::span<const std::byte> safe_in_payload(c_data_ptr, c_size);

                    header.compressed_size = static_cast<uint32_t>(
                        core::smartCompressor::compress(safe_in_payload, out_payload, strategy)
                    );

                    // Safely copy header to buffer
                    std::memcpy(wireBuffer.data(), &header, sizeof(chunkWireHeader));

                    core::Chunk net_chunk{};
                    net_chunk.payloadSize = sizeof(chunkWireHeader) + header.compressed_size;
                    net_chunk.data = std::span<const std::byte>(wireBuffer.data(), net_chunk.payloadSize);

                    transport_.sendChunk(net_chunk);
                } catch (const std::exception& e) {
                    spdlog::error("Thread error: {}", e.what());
                }
            }));
        }

        // Wait safely until all background threads finish compressing
        for (auto& f : futures) {
            f.get();
        }

        spdlog::info("All chunks dispatched to network. waiting for ACKs...");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        spdlog::info("transfer complete.");
    }

    void Session::recvFile(const std::filesystem::path &output_dir, uint16_t port) {
        output_dir_ = output_dir;
        std::filesystem::create_directories(output_dir_);

        spdlog::info("starting receiver session. Listening on port {} ...", port);
        transport_.listen(port);

        while (!trans_complete_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        spdlog::info("file successfully received and verified.");
    }

    void Session::handleRecv(std::vector<std::byte> &&data) {
        if (data.empty()) return;

        auto type = static_cast<messageType>(data[0]);

        if (type == messageType::handshake) {
            if (data.size() >= sizeof(handshake)) {
                // Safely read unaligned memory
                handshake hs{};
                std::memcpy(&hs, data.data(), sizeof(handshake));
                processHandshake(&hs);
            }
        } else if (type == messageType::chunkData) {
            if (data.size() >= sizeof(chunkWireHeader)) {
                // Safely read unaligned memory
                chunkWireHeader header{};
                std::memcpy(&header, data.data(), sizeof(chunkWireHeader));

                if (data.size() < sizeof(chunkWireHeader) + header.compressed_size) return;

                std::span<const std::byte> payload(data.data() + sizeof(chunkWireHeader), header.compressed_size);
                std::vector<std::byte> payload_copy(payload.begin(), payload.end());

                thread_pool_.submit([this, header, payload_copy = std::move(payload_copy)]() {
                    try {
                        processChunk(&header, payload_copy);
                    } catch (const std::exception& e) {
                        spdlog::error("Decompress error: {}", e.what());
                    }
                });
            }
        }
    }

    void Session::processHandshake(const handshake *hs) {
        spdlog::info("Handshake: {} ({} bytes, {} chunks)", hs->file_name, hs->file_size, hs->total_chunks);
        expectedChunks_ = hs->total_chunks;
        auto target_path = output_dir_ / hs->file_name;
        core::FileMapper::preallocate_file(target_path, hs->file_size);
        write_view_ = file_mapper_.map_write(target_path, 0, hs->file_size);
    }

    void Session::processChunk(const chunkWireHeader *header, std::span<const std::byte> payload) {
        if (write_view_.empty()) return;

        size_t offset = static_cast<size_t>(header->chunk_id) * core::Chunker::kDefaultChunkSize;
        if (offset + header->original_size > write_view_.size()) return;

        std::span<std::byte> target_span(write_view_.data() + offset, header->original_size);
        auto strategy = static_cast<core::smartCompressor::Strategy>(header->compress_stra);

        core::smartCompressor::decompress(payload, target_span, strategy);

        uint32_t computed_crc = core::Hasher::crc32(target_span);
        if (computed_crc != header->crc32) {
            spdlog::error("CRC32 mismatch on chunk {}", header->chunk_id);
            return;
        }

        uint32_t received = ++chunks_recv_;
        if (received % 50 == 0 || received == expectedChunks_) {
            spdlog::info("progress: {}/{} chunks received.", received, expectedChunks_);
        }

        if (received == expectedChunks_) {
            trans_complete_ = true;
        }
    }

}