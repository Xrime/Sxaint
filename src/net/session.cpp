//
// Created by xint2 on 05/07/2026.
//
#include "../include/net/session.h"
#include "../include/core/chunker.h"
#include "../include/core/compressor.h"
#include "../include/core/hasher.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <iostream>
#include <random>

namespace sxaint::net {

    Session::Session() : thread_pool_(0) {
        transport_.setRecvCallback([this](std::vector<std::byte>&& data) {
            this->handleRecv(std::move(data));
        });
    }

    void Session::sendFile(const std::filesystem::path &file_path, const std::string &target_ip, uint16_t port, uint32_t pin) {
        spdlog::info("Starting sender session for {}", file_path.filename().string());

        uint64_t file_size = std::filesystem::file_size(file_path);
        metrics_ = std::make_unique<core::transferMetrics>(file_size);
        auto file_view = file_mapper_.map_read(file_path, 0, file_size);


        transport_.connect(target_ip, port);

        handshake hs{};
        hs.type = static_cast<uint8_t>(messageType::handshake);
        hs.pin = pin;
        hs.file_size = file_size;
        hs.chunk_size = core::Chunker::kDefaultChunkSize;

        // auto chunks = core::Chunker::slice(file_view, hs.chunk_size);
        hs.total_chunks = static_cast<uint32_t>((file_size + hs.chunk_size-1)/hs.chunk_size);// cacl the chunks

        auto u8_name= file_path.filename().u8string();
        std::string filename(u8_name.begin(), u8_name.end());
        std::strncpy(hs.file_name, filename.c_str(), sizeof(hs.file_name)-1);

        core::Chunk hs_chunk{};
        // hs_chunk.payloadSize = sizeof(handshake);
        // hs_chunk.data = std::span<const std::byte>(reinterpret_cast<const std::byte*>(&hs), sizeof(handshake));
        // transport_.sendChunk(hs_chunk);
        std::vector<std::byte> hs_buffer(sizeof(handshake));
        std::memcpy(hs_buffer.data(), &hs, sizeof(handshake));
        transport_.sendChunk(std::move(hs_buffer));
        spdlog::info("handshake sent. Waiting for receiver to acknowledge resume state");
        while (!handshake_acked_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (handshake_rejected_) {
            spdlog::error("Transfer aborted. The PIN you entered was incorrect. ");
            return;
        }
        

        spdlog::info("ACK recv.Slicing file for transfer");
        auto chunks = core::Chunker::slice(file_view, hs.chunk_size);
        auto sample_size = std::min<size_t>(4096, file_size);
        auto strategy = core::smartCompressor::detect(file_path, std::span<const std::byte>(file_view.data(), sample_size));
        spdlog::info("Compression strategy selected: {}", static_cast<int>(strategy));

        std::vector<std::future<void>> futures;
        uint32_t skipped_chunks = 0;
        for (const auto& chunk : chunks) {

            if (chunk.id < resume_bitfield_.size() && resume_bitfield_[chunk.id]) {
                skipped_chunks++;
                chunks_sent_++;
            }
            uint32_t stream_id = chunk.id % 4;
            while (transport_.get_wait_snd() > 32000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            uint32_t c_id = chunk.id;
            uint32_t c_size = chunk.payloadSize;
            uint32_t c_crc32 = chunk.crc32;
            const std::byte* c_data_ptr = chunk.data.data();

            futures.push_back(thread_pool_.submit([this, strategy, c_id, c_size, c_crc32, c_data_ptr, stream_id]() {
                try {
                    size_t max_bound = core::smartCompressor::get_compress_bound(c_size, strategy);
                    std::vector<std::byte> wireBuffer(sizeof(chunkWireHeader) + max_bound);

                    chunkWireHeader header{};
                    header.type = static_cast<uint8_t>(messageType::chunkData);
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
                    wireBuffer.resize(sizeof(chunkWireHeader)+ header.compressed_size);
                    // core::Chunk net_chunk{};
                    // net_chunk.payloadSize = sizeof(chunkWireHeader) + header.compressed_size;
                    // net_chunk.data = std::span<const std::byte>(wireBuffer.data(), net_chunk.payloadSize);
                    //
                    // transport_.sendChunk(net_chunk);
                    transport_.sendChunk(std::move(wireBuffer), stream_id);
                    chunks_sent_.fetch_add(1, std::memory_order_relaxed);
                    if (metrics_) {
                        metrics_->add_bytes(c_size);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Thread error: {}", e.what());
                }
            }));
        }

        // Wait safely until all background threads finish compressing
        // for (auto& f : futures) {
        //     f.get();
        // }
        //
        // spdlog::info("All chunks dispatched to network. waiting for ACKs...");
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        // spdlog::info("transfer complete.");
        if (skipped_chunks>0) {
            spdlog::info("Resuming Transfer: Skipped {} chunks that already exist.", skipped_chunks);
        }
        uint32_t total = chunks.size();
        while (chunks_sent_ < total) {
            // int percent = (chunks_sent_ * 100)/total;
            // std::cout << "\r[SENDER]   [" << std::string(percent / 2, '#') << std::string(50 - percent / 2, ' ') << "] " << percent << "% " << std::flush;
            // std::this_thread::sleep_for(std::chrono::milliseconds(20));

            if (metrics_) {
                std::cout << "\r"<<filename << " "<< metrics_->get_ui_string()<< std::flush;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        }
        if (metrics_) {
            std::cout<<"\r"<< filename<<" "<<metrics_->get_ui_string()<<"\n"<< std::flush;
        }
        for (auto& f : futures) {
            if (f.valid()) {
                f.get();
            }
        }
        // std::cout << "\r[SENDER]   [" << std::string(50, '#') << "] 100%\n" << std::flush;
        // for (auto& f :futures) {
        //     f.get();
        // }
        spdlog::info("Transfer complete. Closing buffers.");
    }

    void Session::recvFile(const std::filesystem::path &output_dir, uint16_t port) {
        output_dir_ = output_dir;
        std::filesystem::create_directories(output_dir_);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib(100000,999999);
        expected_pin_ = distrib(gen);

        spdlog::info("Receiver Ready. Your secure PIN is: {}", expected_pin_);


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
                thread_pool_.submit([this, hs]() {
                    try {
                        processHandshake(&hs);
                    }catch (const std::exception& e) {
                        spdlog::error("Handshake thread error: {}", e.what());
                    }
                });
            }
        }else if (type == messageType::handshakeAck){
            processHandshakeACk(data);
        }else if (type ==messageType::handshakeReject) {
            spdlog::error("handshake REJECTED by Receiver! Incorrect PIN.");
            handshake_rejected_ = true;
            handshake_acked_ =true;
        }
        else if (type == messageType::chunkData) {
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

        if (hs->pin != expected_pin_) {
            spdlog::error("Unauthorized transfer attempt! Invalid PiN: {}", hs->pin);
            std::vector<std::byte> reject_buffer(1);
            reject_buffer[0] = static_cast<std::byte>(messageType::handshakeReject);
            transport_.sendChunk(std::move(reject_buffer), 0);
            return;
        }
        current_filename_ = hs->file_name;
        expectedChunks_ = hs->total_chunks;
        metrics_ = std::make_unique<core::transferMetrics>(hs->file_size);
        std::u8string u8_target(current_filename_.begin(), current_filename_.end());
        std::filesystem::path safe_filename(u8_target);
        auto target_path = output_dir_ / current_filename_;

        current_manifest_.fileHash = current_filename_;
        current_manifest_.fileSize = hs->file_size;
        current_manifest_.chunk_size = hs->chunk_size;
        current_manifest_.total_chunks = hs->total_chunks;
        current_manifest_.completedChunks.assign(hs->total_chunks, false);
        chunks_recv_ = 0;
        
        spdlog::info("Handshake: {} ({} bytes, {} chunks)", hs->file_name, hs->file_size, hs->total_chunks);
        spdlog::info("Instantly allocating {} via Sparse File...", current_filename_);
        
        core::FileMapper::preallocate_file(target_path, hs->file_size);

        // Unconditionally map the file
        write_view_ = file_mapper_.map_write(target_path, 0, hs->file_size);

        auto packed_bits = current_manifest_.pack_bitfield();
        std::vector<std::byte> ack_buffer(sizeof(handshakeAck) + packed_bits.size());
        handshakeAck ack{};
        ack.type = static_cast<uint8_t>(messageType::handshakeAck);
        ack.total_chunks = hs->total_chunks;
        std::memcpy(ack_buffer.data(), &ack, sizeof(handshakeAck));
        std::memcpy(ack_buffer.data() + sizeof(handshakeAck), packed_bits.data(), packed_bits.size());

        transport_.sendChunk(std::move(ack_buffer));
    }
    void Session::processHandshakeACk(const std::vector<std::byte> &data) {
        if (data.size()< sizeof(handshakeAck)) return;
        handshakeAck ack{};
        std::memcpy(&ack, data.data(), sizeof(handshakeAck));
        size_t bitfield_size = data.size() - sizeof(handshakeAck);
        std::vector<uint8_t> packed_bitfield(bitfield_size);
        std::memcpy(packed_bitfield.data(), data.data()+ sizeof(handshakeAck), bitfield_size);
        resume_bitfield_.assign(ack.total_chunks, false);
        for (uint32_t i = 0; i < ack.total_chunks; ++i) {
            resume_bitfield_[i] = (packed_bitfield[i / 8] & (1 << (i % 8))) != 0;
        }
        handshake_acked_ = true;
    }


    void Session::processChunk(const chunkWireHeader *header, std::span<const std::byte> payload) {
        if (write_view_.empty()) {
            spdlog::error("Write view is empty, dropping chunk {}", header->chunk_id);
            return;
        }
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
        {
            std::lock_guard<std::mutex> lock(manifest_mutex_);
            if (!current_manifest_.completedChunks[header->chunk_id]) {
                current_manifest_.completedChunks[header->chunk_id] = true;
            }
        }
        // uint32_t received = chunks_recv_.fetch_add(1,std::memory_order_relaxed)+1;
        // int percent = (received * 100)/ expectedChunks_;
        // std::cout << "\r[RECEIVER] [" << std::string(percent / 2, '=') << std::string(50 - percent / 2, ' ') << "] " << percent << "% " << std::flush;
        if (metrics_) {
            metrics_->add_bytes(header->original_size);

        }
        uint32_t received =chunks_recv_.fetch_add(1,std::memory_order_relaxed)+1;
        if (metrics_) {
            std::cout << "\r" << current_filename_<< " " <<metrics_->get_ui_string() <<std::flush;

        }
        if (received == expectedChunks_) {
            std::cout<< "\n";
            trans_complete_= true;
        }
    }

}
