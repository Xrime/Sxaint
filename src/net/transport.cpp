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

        ikcp_nodelay(kcp_,config.nodelay,config.interval, config.resend, config.nc);
        ikcp_wndsize(kcp_, config.send_wnd, config.recv_wnd);
        ikcp_setmtu(kcp_, config.mtu);
        running_= true;
        io_thread_ = std::jthread(&KCPTransport::net_loop, this);
        spdlog::info("KCP Transport connected to {}:{}", host,port);
    }



}