//
// Created by xint2 on 04/07/2026.
//

#ifndef SXAINT_TRANSPORT_H
#define SXAINT_TRANSPORT_H
#include "../core/chunker.h"
#include <string>
#include <functional>
#include <vector>
#include  <mutex>
#include <atomic>
#include <thread>
#include <winsock2.h>
#include  <ws2tcpip.h>

struct IKCPCB;
namespace sxaint::net {
    class KCPTransport {
    public:
        struct Config {
            uint32_t conv_id = 0x11223344;
            int send_wnd = 16384;
            int recv_wnd = 16384;
            int nodelay = 1;
            int interval = 10;
            int resend = 2;
            int nc = 1;
            int mtu = 1400;

            Config(){}
        };
        struct Stats {
            double throughput_mbps{0.0};
            uint64_t bytes_sent{0};
            uint64_t bytes_received{0};
        };
        KCPTransport();
        ~KCPTransport();
        int get_wait_snd();
        KCPTransport(const KCPTransport&) = delete;
        KCPTransport& operator =(const KCPTransport&) = delete;
        void connect(const std::string& host, uint16_t port, const Config& config = Config{});
        void listen(uint16_t port, const Config& config = Config{});
        // void sendChunk(const core::Chunk& chunk);
        void sendChunk(std::vector<std::byte>&& raw_payload);
        using onChunkReceived = std::function<void(std::vector<std::byte>&&)>;
        void setRecvCallback(onChunkReceived cb);
        Stats get_stats() const;


    private:
        static int kcpOutputCallback(const char* buf, int len, IKCPCB* kcp, void* user);
        void net_loop();
        void init_winsock();

        SOCKET socket_{INVALID_SOCKET};
        sockaddr_in remote_addr_{};
        IKCPCB* kcp_{nullptr};

        std::jthread io_thread_;
        std::atomic<bool> running_{false};
        std::mutex kcpMutex_;
        onChunkReceived on_received_;
        std::atomic<uint64_t> byteSent_{0};
        std::atomic<uint64_t> byteReceived_{0};

    };
}
#endif //SXAINT_TRANSPORT_H