//
// Created by xint2 on 05/07/2026.
//

#ifndef SXAINT_SESSION_H
#define SXAINT_SESSION_H
#include "transport.h"
#include "protocol.h"
#include "../core/file_mapper.h"
#include "../core/thread_pool.h"
#include <filesystem>
#include <atomic>
#include <memory>

namespace sxaint::net {
    class Session {
    public:
        Session();
        ~Session() = default;

        // run as sender
        void sendFile(const std::filesystem::path& file_path, const std::string& target_ip, uint16_t port);
        void recvFile(const std::filesystem::path& output_dir, uint16_t port);// run as receiver

    private:
        void handleRecv(std::vector<std::byte>&& data);
        void processHandshake(const handshake* hs);
        void processChunk(const chunkWireHeader* header, std::span<const std::byte> payload);

        KCPTransport transport_;
        core::ThreadPool thread_pool_;
        core::FileMapper file_mapper_;

        std::filesystem::path output_dir_;
        std::atomic<uint32_t> chunks_recv_{0};
        uint32_t expectedChunks_{0};
        bool trans_complete_{false};

        std::span<std::byte> write_view_;
    };
}
#endif //SXAINT_SESSION_H