//
// Created by xint2 on 12/07/2026.
//

#ifndef SXAINT_STUN_H
#define SXAINT_STUN_H
#include <cstdint>
#include <string>

namespace  sxaint::net {
    struct publicEndpoint {
        std::pmr::string ip;
        uint16_t port;
        bool success;
    };
    class stunClient {
    public:
        static publicEndpoint getPublicEndpoint(uint16_t local_port);
    };
}
#endif //SXAINT_STUN_H