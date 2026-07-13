//
// Created by xint2 on 12/07/2026.
//
#include <winsock2.h>
#include "../../include/net/stun.h"
#include <ws2tcpip.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
namespace sxaint::net {
    publicEndpoint stunClient::getPublicEndpoint(uint16_t local_port) {
        publicEndpoint endpoint{"", 0, false};

        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
               return endpoint;
             }
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            return endpoint ;
        }
        DWORD timeout = 2000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(local_port);
        local_addr.sin_addr.s_addr = INADDR_ANY;
        bind(sock, (sockaddr*)&local_addr, sizeof(local_addr));
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo("stun.l.google.com", "19302", &hints, &res) != 0) {
            closesocket(sock);
            WSACleanup();
            return endpoint;
        }

        std::vector<uint8_t> stunReq(20,0);
        stunReq[0] =0x00;
        stunReq[1] =0x01;
        stunReq[4] = 0x21;
        stunReq[5] =0x12;
        stunReq[6] =0xA4;
        stunReq[7] = 0x42;

        sendto(sock,(const char*)stunReq.data(), stunReq.size(), 0, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        std::vector<uint8_t>stunResp(1024);
        sockaddr_in from_addr{};
        int from_len= sizeof(from_addr);
        int bytes = recvfrom(sock, (char*)stunResp.data(), stunResp.size(),0,(sockaddr*)&from_addr, &from_len);
        closesocket(sock);
        if (bytes > 20) {
            for (int i = 20;i<bytes;) {
                uint16_t attr_type = (stunResp[i] << 8) | stunResp[i+1];
                uint16_t attr_len = (stunResp[i+2] << 8) | stunResp[i+3];
                if (attr_type == 0x0001 || attr_type == 0x0020) { // XOR
                    uint16_t port = (stunResp[i+6] << 8) | stunResp[i+7];
                    uint32_t ip = (stunResp[i+8] << 24) | (stunResp[i+9] << 16) | (stunResp[i+10] << 8) | stunResp[i+11];
                    if (attr_type==0x0020) {
                        port ^= 0x2112;
                        ip ^= 0x2112A442;
                    }
                    endpoint.port = port;
                    endpoint.ip =std::to_string((ip >> 24) & 0xFF) + "." + std::to_string((ip >> 16) & 0xFF) + "." + std::to_string((ip >> 8) & 0xFF) + "." + std::to_string(ip & 0xFF);
                    endpoint.success = true;
                    break;;
                }
                i +=4 + attr_len;
            }

        }
        WSACleanup();
        return endpoint;
    }
}