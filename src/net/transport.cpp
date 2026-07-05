//
// Created by xint2 on 04/07/2026.
//
#include "../include/net/transport.h"
#include <ikcp.h>
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
        if (kcp_) {
            ikcp_release(kcp_);
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
        auto* transport =static_cast<KCPTransport*>(user);
        int sent = sendto(transport->socket_, buf, len,0,
            reinterpret_cast<sockaddr*>(&transport->remote_addr_),
            sizeof(transport->remote_addr_));
        if (sent>0) {
            transport->byteSent_ +=sent;
        }
        return 0;
    }
    void KCPTransport::connect(const std::string &host, uint16_t port, const Config &config) {
        socket_= socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            throw std::runtime_error("failed to create UdP socket");
        }
        int buf_size = 8*1024*1024;
        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&buf_size), sizeof(buf_size));
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&buf_size),sizeof (buf_size));

        remote_addr_.sin_family = AF_INET;
        remote_addr_.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &remote_addr_.sin_addr);
        kcp_ = ikcp_create(config.conv_id, this);
        kcp_->output = kcpOutputCallback;
        kcp_->stream = 1;

        ikcp_nodelay(kcp_,config.nodelay,config.interval, config.resend, config.nc);
        ikcp_wndsize(kcp_, config.send_wnd, config.recv_wnd);
        ikcp_setmtu(kcp_, config.mtu);
        running_= true;
        io_thread_ = std::jthread(&KCPTransport::net_loop, this);
        spdlog::info("KCP Transport connected to {}:{}", host,port);
    }
    void KCPTransport::listen(uint16_t port, const Config &config) {
        socket_= socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (socket_ ==INVALID_SOCKET) {
            throw std::runtime_error("Failed to create UDP socket");
        }
        int buf_size = 8*1024*1024;
        setsockopt(socket_,SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&buf_size), sizeof(buf_size));
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&buf_size), sizeof(buf_size));

        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port =htons(port);
        local_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(socket_, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr))== SOCKET_ERROR) {
            throw std::runtime_error("failed to bind UDP socket");

        }
        kcp_ = ikcp_create(config.conv_id, this);
        kcp_->output= kcpOutputCallback;
        kcp_->stream = 1;
        ikcp_nodelay(kcp_, config.nodelay, config.interval, config.resend, config.nc);
        ikcp_wndsize(kcp_,config.send_wnd, config.recv_wnd);
        ikcp_setmtu(kcp_, config.mtu);

        running_= true;
        io_thread_ = std::jthread(&KCPTransport::net_loop, this);
        spdlog::info("KCP transport listen on port{}", port);
    }
    void KCPTransport::sendChunk(const core::Chunk &chunk) {
        // //later to tag chunk header to data, fornoew raw date
        // int ret = ikcp_send(kcp_, reinterpret_cast<const char *>(chunk.data.data()), static_cast<int>(chunk.payloadSize));
        // if (ret < 0) {
        //     spdlog::error("KCP send failed: {}", ret);
        std::lock_guard<std::mutex> lock(kcpMutex_);
        //send a 4-bytes data so that the receiver know how much data is coming
        uint32_t len = static_cast<uint32_t>(chunk.payloadSize);
        ikcp_send(kcp_, reinterpret_cast<const char *>(&len), 4);
        int ret =ikcp_send(kcp_,reinterpret_cast<const char *>(chunk.data.data()), static_cast<int>(len));
        if (ret <0) {
            spdlog::error("KCP send failed: {}", ret);
        }

    }

    void KCPTransport::setRecvCallback(onChunkReceived cb) {
        on_received_ = std::move(cb);
    }
    void KCPTransport::net_loop() {
        std::vector<char> recv_buf(65536);
        std::vector<std::byte> stream_buffer;
        u_long mode =1;
        ioctlsocket(socket_, FIONBIO, &mode);
        bool address_locked = false;
        while (running_) {
            auto now = static_cast<IUINT32>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
            sockaddr_in sender_addr{};
            int sender_len = sizeof(sender_addr);

            while (true) {
                int bytes_read = recvfrom(socket_, recv_buf.data(), static_cast<int>(recv_buf.size()),0,
                                            reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);
                if (bytes_read >0) {
                    std::lock_guard<std::mutex> lock(kcpMutex_);
                    ikcp_input(kcp_, recv_buf.data(), bytes_read);
                    if (!address_locked && remote_addr_.sin_family == 0) {
                        remote_addr_ = sender_addr;
                        address_locked = true;
                    }
                }else break;

            }
            {
                std::lock_guard<std::mutex> lock(kcpMutex_);
                ikcp_update(kcp_,now);
            }
            //reassembled
            while (true) {
                kcpMutex_.lock();
                int size = ikcp_recv(kcp_, recv_buf.data(), static_cast<int>(recv_buf.size()));
                kcpMutex_.unlock();
                //std::lock_guard<std::mutex> lock(kcpMutex_);
                if (size>0) {
                    byteReceived_ += size;
                    stream_buffer.insert(stream_buffer.end(),
                        reinterpret_cast<std::byte*>(recv_buf.data()),
                        reinterpret_cast<std::byte *>(recv_buf.data())+size
                        );
                    while (stream_buffer.size() >=4) {
                        uint32_t frameLength = 0;
                        std::memcpy(&frameLength, stream_buffer.data(),4);

                        if (stream_buffer.size() >= 4 + frameLength) {
                            std::vector<std::byte> final_data(frameLength);
                            std::memcpy(final_data.data(), stream_buffer.data()+ 4 ,frameLength);
                            stream_buffer.erase(stream_buffer.begin(), stream_buffer.begin() + 4 + frameLength);
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
            std::this_thread::sleep_for(std::chrono::milliseconds(2));


        }
    }
    KCPTransport::Stats KCPTransport::get_stats() const {
        Stats s;
        s.bytes_sent = byteSent_.load();
        s.bytes_received = byteReceived_.load();
        return s;
    }
}
