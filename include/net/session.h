//
// Created by xint2 on 05/07/2026.
//

#ifndef SXAINT_SESSION_H
#define SXAINT_SESSION_H
#include "transport.h"
#include "../core/manifest.h"
#include "protocol.h"
#include "../core/file_mapper.h"
#include "../core/thread_pool.h"
#include <filesystem>
#include <atomic>
#include <memory>
#include <mutex>
#include <future>
#include "../core/metrics.h"
#include "../core/crypto.h"
#include <functional>




namespace sxaint::net {
    class Session {
    public:
        Session();
        ~Session() = default;

        // run as sender
        void sendFile(const std::filesystem::path& file_path, const std::string& target_ip, uint16_t port, uint32_t pin, std::function<void(int percent, double mbps, uint32_t eta)> on_progress=nullptr);
        void recvFile(const std::filesystem::path& output_dir, uint16_t port, uint32_t expected_pin, std::function<void(int percent, double mbps, uint32_t eta)> on_progress = nullptr);// run as receiver

    private:
        std::vector<std::byte> aes_key_;
        void handleRecv(std::vector<std::byte>&& data);
        void processHandshake(const handshake* hs);
        void processHandshakeACk(const std::vector<std::byte>& data);
        void processChunk(const chunkWireHeader* header, std::span<const std::byte> payload);


        KCPTransport transport_;
        core::ThreadPool thread_pool_;
        core::FileMapper file_mapper_;
        std::filesystem::path output_dir_;
        std::string current_filename_;
        std::span<std::byte> write_view_;
        std::atomic<bool> handshake_rejected_{false};
        uint32_t expected_pin_{0};
        std::atomic<uint32_t> chunks_recv_{0};
        std::atomic<uint32_t> expectedChunks_{0};
        std::atomic<bool> trans_complete_{false};
        std::atomic<uint32_t> chunks_sent_{0};

        std::atomic<bool> handshake_acked_{false};
        std::vector<bool> resume_bitfield_;
        core::transferManifest current_manifest_;
        std::mutex manifest_mutex_;
        std::unique_ptr<core::transferMetrics> metrics_;
        std::function<void(int percent, double mbps, uint32_t eta)> on_progress_;

    };
}
#endif //SXAINT_SESSION_H