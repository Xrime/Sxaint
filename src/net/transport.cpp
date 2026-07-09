//
// Created by xint2 on 04/07/2026.
//
#include "../include/net/transport.h"
#include "ikcp.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
namespace sxaint::net {
    KCPTransport::KCPTransport() {
        init_winsock();
    }
    KCPTransport::~KCPTransport() {
         running_ = false;
        if (io_thread_.joinable()){
            io_thread_.join();
        }
        // if (kcp_) {
        //     ikcp_release(kcp_);
        // }
        for (int i = 0; i < kNumStreams; ++i) {
            if (kcps_[i]) {
                ikcp_release(kcps_[i]);
                kcps_[i] = nullptr;
            }
        }
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
        }
        WSACleanup();
    }
    void KCPTransport::init_winsock() {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
        if (result != 0) {
            throw std::runtime_error(fmt::format("WSAStartup failed: {}", result));


        }

    }int KCPTransport::kcpOutputCallback(const char *buf, int len, IKCPCB *kcp, void *user) {
        auto* pair = static_cast<std::pair<KCPTransport*, int>*> (user);
        auto* transport =pair->first;
        int sent = sendto(transport->socket_, buf, len,0,
            reinterpret_cast<sockaddr*>(&transport->remote_addr_),
            sizeof(transport->remote_addr_));
        if (sent>0) {
            transport->byteSent_.fetch_add(sent, std::memory_order_relaxed);
        }
        return 0;
    }
    void KCPTransport::connect(const std::string &host, uint16_t port, const Config &config) {
        socket_= socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            throw std::runtime_error("failed to create UdP socket");
        }
        int buf_size = 32*1024*1024;
        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&buf_size), sizeof(buf_size));
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&buf_size),sizeof (buf_size));

        remote_addr_.sin_family = AF_INET;
        remote_addr_.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &remote_addr_.sin_addr);

        for (int i = 0; i< kNumStreams; ++i) {
            kcps_[i] = ikcp_create(config.conv_id+i, this);
            ikcp_nodelay(kcps_[i], config.nodelay, config.interval, config.resend, config.nc);
            ikcp_wndsize(kcps_[i], 4096, 4096);
            ikcp_setmtu(kcps_[i], 16384);
            kcps_[i]->user = new std::pair<KCPTransport*, int>(this,i);
            kcps_[i]->output = kcpOutputCallback;
            kcps_[i]->stream = 1;
        }

        // ikcp_nodelay(kcp_,config.nodelay,config.interval, config.resend, config.nc);
        // ikcp_wndsize(kcp_, config.send_wnd, config.recv_wnd);
        // ikcp_setmtu(kcp_, config.mtu);
        running_= true;
        io_thread_ = std::jthread(&KCPTransport::net_loop, this);
        spdlog::info("KCP Transport connected to {}:{}", host,port);
    }
    void KCPTransport::listen(uint16_t port, const Config &config) {
        socket_= socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (socket_ ==INVALID_SOCKET) {
            throw std::runtime_error("Failed to create UDP socket");
        }
        int buf_size = 32*1024*1024;
        setsockopt(socket_,SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&buf_size), sizeof(buf_size));
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&buf_size), sizeof(buf_size));

        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port =htons(port);
        local_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(socket_, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr))== SOCKET_ERROR) {
            throw std::runtime_error("failed to bind UDP socket");

        }

        for (int i = 0; i< kNumStreams; ++i) {
            kcps_[i] = ikcp_create(config.conv_id + i, this);
            ikcp_nodelay(kcps_[i], config.nodelay, config.interval, config.resend, config.nc);
            ikcp_wndsize(kcps_[i], 4096, 4096);
            ikcp_setmtu(kcps_[i], 16384);
            kcps_[i]->user = new std::pair<KCPTransport*, int>(this, i);
            kcps_[i]->output = kcpOutputCallback;
            kcps_[i]->stream = 1;
        }
        //
        // ikcp_nodelay(kcp_, config.nodelay, config.interval, config.resend, config.nc);
        // ikcp_wndsize(kcp_,config.send_wnd, config.recv_wnd);
        // ikcp_setmtu(kcp_, config.mtu);

        running_= true;
        io_thread_ = std::jthread(&KCPTransport::net_loop, this);
        spdlog::info("KCP transport listen on port{}", port);
    }
    void KCPTransport::sendChunk(std::vector<std::byte> &&raw_payload, uint32_t stream_id) {
        uint32_t len = static_cast<uint32_t>(raw_payload.size());
        uint32_t s_id = stream_id % kNumStreams;

        raw_payload.insert(raw_payload.begin(), reinterpret_cast<std::byte*>(&len), reinterpret_cast<std::byte*>(&len) + 4);

        int total_size = static_cast<int>(raw_payload.size());
        const char* ptr = reinterpret_cast<const char*>(raw_payload.data());
        int sent = 0;
        int max_send_size = 128 * 1024; // 128 KB max per ikcp_send to respect IKCP_WND_RCV limit

        while (sent < total_size) {
            int to_send = std::min(max_send_size, total_size - sent);
            int ret = ikcp_send(kcps_[s_id], ptr + sent, to_send);
            if (ret < 0) {
                spdlog::error("KCP send failed: {}", ret);
                break;
            }
            sent += to_send;
        }
        ikcp_flush(kcps_[s_id]);
    }
    int KCPTransport::get_wait_snd(uint32_t stream_id) {
        uint32_t s_id = stream_id % kNumStreams;
        std::lock_guard<std::mutex> lock(kcpMutexes_[s_id]);
        return kcps_[s_id] ? ikcp_waitsnd(kcps_[s_id]) : 0;
    }

    void KCPTransport::setRecvCallback(onChunkReceived cb) {
        on_received_ = std::move(cb);
    }
    void KCPTransport::net_loop() {
        std::vector<char> recv_buf(65536);
        std::vector<std::byte> stream_buffer[kNumStreams];
        u_long mode =1;
        ioctlsocket(socket_, FIONBIO, &mode);
        bool address_locked = false;
        while (running_) {
            bool active = false;
            auto now = static_cast<IUINT32>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
            sockaddr_in sender_addr{};
            int sender_len = sizeof(sender_addr);

            while (true) {
                int bytes_read = recvfrom(socket_, recv_buf.data(), static_cast<int>(recv_buf.size()),0,reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);
                if (bytes_read > 4) {
                    IUINT32 conv = ikcp_getconv(recv_buf.data());
                    uint32_t s_id = conv % kNumStreams;
                    std::lock_guard<std::mutex> lock(kcpMutexes_[s_id]);
                    if (!address_locked && remote_addr_.sin_family == 0) {
                        remote_addr_ = sender_addr;
                        address_locked = true;
                    }
                    ikcp_input(kcps_[s_id], recv_buf.data(), bytes_read);
                    ikcp_flush(kcps_[s_id]);
                    active = true;
                } else break;

            }
            int pending_snd = 0;
            for ( int i = 0; i< kNumStreams; ++i){
                std::lock_guard<std::mutex> lock(kcpMutexes_[i]);
                if (kcps_[i]) {
                    ikcp_update(kcps_[i],now);
                    pending_snd += ikcp_waitsnd(kcps_[i]);
                }
            }
            //reassembled
            for (int i = 0; i < kNumStreams; ++i) {
                while (true) {
                    kcpMutexes_[i].lock();
                    int size = ikcp_recv(kcps_[i], recv_buf.data(), static_cast<int>(recv_buf.size()));
                    kcpMutexes_[i].unlock();
                    //std::lock_guard<std::mutex> lock(kcpMutex_);
                    if (size>0) {
                        active = true;
                        byteReceived_.fetch_add(size,std::memory_order_relaxed);
                        stream_buffer[i].insert(stream_buffer[i].end(),
                            reinterpret_cast<std::byte*>(recv_buf.data()),
                            reinterpret_cast<std::byte *>(recv_buf.data())+size
                            );
                        while (stream_buffer[i].size() >=4) {
                            uint32_t frameLength = 0;
                            std::memcpy(&frameLength, stream_buffer[i].data(),4);

                            if (stream_buffer[i].size() >= 4 + frameLength) {
                                std::vector<std::byte> final_data(frameLength);
                                std::memcpy(final_data.data(), stream_buffer[i].data()+ 4 ,frameLength);
                                stream_buffer[i].erase(stream_buffer[i].begin(), stream_buffer[i].begin() + 4 + frameLength);
                                if (on_received_) {
                                    on_received_(std::move(final_data));
                                }
                            }else {
                                break;
                            }
                        }
                    }else {
                        break;
                    }
                }
            }
                if (!active && pending_snd == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else if (!active) {
                    std::this_thread::yield();
                }

            }
    }
    KCPTransport::Stats KCPTransport::get_stats() const {
        Stats s;
        s.bytes_sent = byteSent_.load();
        s.bytes_received = byteReceived_.load();
        return s;
    }
}
