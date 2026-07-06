//
// Created by xint2 on 06/07/2026.
//
#include "../include/net/discovery.h"
#include <spdlog/spdlog.h>
#include  <vector>

#pragma  comment(lib, "ws2_32.lib")

namespace sxaint::net {
    Discovery::Discovery() {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2,2),&wsa_data);
        }
    Discovery::~Discovery() {
        stop();
        WSACleanup();
        }
    void Discovery::set_callback(peerCallback cb) {
        on_peer_found_ = std::move(cb);
    }
    void Discovery::start(uint16_t port, const std::string &deviceName) {
        port_ = port;
        deviceName_ = deviceName;
        running_= true;
        broadcastSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int broadcastEnable = 1;
        setsockopt(broadcastSocket_,SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&broadcastEnable), sizeof(broadcastEnable));
        listenSocket_= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int reuse = 1;
        setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse), sizeof(reuse));
        sockaddr_in listen_addr{};
        listen_addr.sin_family = AF_INET;
        listen_addr.sin_port = htons(port_);
        listen_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&listen_addr), sizeof(listen_addr)) == SOCKET_ERROR) {
            spdlog::error("failed to bind discovery listen socket. ");
            return;
        }
        broadcast_thread_ = std::jthread(&Discovery::broadcast_loop, this);
        listen_thread_ = std::jthread(&Discovery::listen_loop, this);
        spdlog::info("Discovery service. Broadcasting as '{}'", deviceName_);

    }
    void Discovery::stop() {
        running_=false;
        if (broadcastSocket_!= INVALID_SOCKET) {
            closesocket(broadcastSocket_);
            broadcastSocket_= INVALID_SOCKET;
        }
        if (listenSocket_ != INVALID_SOCKET) {
            closesocket(listenSocket_);
            listenSocket_= INVALID_SOCKET;
        }
    }
    void Discovery::broadcast_loop() {
        sockaddr_in broadcast__addr{};
        broadcast__addr.sin_family = AF_INET;
        broadcast__addr.sin_port = htons(port_);
        broadcast__addr.sin_addr.s_addr = INADDR_BROADCAST;
        std::string payload = "Sxaint|"+deviceName_;
        while (running_) {
            sendto(broadcastSocket_,payload.c_str(), static_cast<int>(payload.length()),0, reinterpret_cast<sockaddr*>(&broadcast__addr), sizeof(broadcast__addr));
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    void Discovery::listen_loop() {
        std::vector<char> buffer(1024);
        sockaddr_in sender_addr{};
        int sender_len = sizeof(sender_addr);

        while (running_) {
            int bytes_read = recvfrom(listenSocket_,buffer.data(), static_cast<int>(buffer.size()) -1,0, reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);
            if (bytes_read>0) {
                buffer[bytes_read] = '\0';
                std::string msg(buffer.data());

                if (msg.starts_with("sxaint|")) {
                    std::string hostname = msg.substr(7);
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sender_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
                    std::string ip(ip_str);
                    std::lock_guard<std::mutex> lok(peers_mutex_);
                    auto now = std::chrono::steady_clock::now();
                    if (active_peers_.find(ip) == active_peers_.end()) {
                        Peer new_peer{ ip, hostname, now};
                        active_peers_[ip] = new_peer;
                        if (on_peer_found_) {
                            on_peer_found_(new_peer);
                        }
                    }else {
                        active_peers_[ip].lastSeen = now;
                    }
                }
            }
        }
    }





}