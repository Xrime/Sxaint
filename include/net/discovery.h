//
// Created by xint2 on 06/07/2026.
//

#ifndef SXAINT_DISCOVERY_H
#define SXAINT_DISCOVERY_H
#include  <string>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace sxaint::net {
    struct  Peer {
        std::string ip_address;
        std::string hostname;
        std::chrono::steady_clock::time_point lastSeen;
    };
    class Discovery {
    public:
        using peerCallback = std::function<void(const Peer&)>;
        Discovery();
        ~Discovery();

        void start(uint16_t port, const std::string& deviceName);
        void stop();
        void set_callback(peerCallback cb);

    private:
        void broadcast_loop();
        void listen_loop();

        std::string deviceName_;
        uint16_t port_;
        std::atomic<bool> running_{false};
        SOCKET broadcastSocket_{INVALID_SOCKET};
        SOCKET listenSocket_{INVALID_SOCKET};
        std::jthread broadcast_thread_;
        std::jthread listen_thread_;
        peerCallback on_peer_found_;
        std::unordered_map<std::string, Peer> active_peers_;
        std::mutex peers_mutex_;
    };
}
#endif //SXAINT_DISCOVERY_H