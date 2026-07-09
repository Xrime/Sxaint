//
// Created by xint2 on 05/07/2026.
//

#ifndef SXAINT_PROTOCOL_H
#define SXAINT_PROTOCOL_H
#include <cstdint>

namespace sxaint::net {

    enum class messageType: uint8_t {
        handshake = 0x01,
        handshakeAck  = 0x02,
        handshakeReject = 0x03,
        chunkData = 0x03

    };
#pragma pack(push, 1)
    struct handshake {
        uint8_t type = static_cast<uint8_t>(messageType::handshake);
        uint32_t pin;
        uint64_t file_size;
        uint32_t chunk_size;
        uint32_t total_chunks;
        char file_name[256];
    };
    struct handshakeAck {
        uint8_t type = static_cast<uint8_t>(messageType::handshakeAck);
        uint32_t total_chunks;
    };
    struct chunkWireHeader {
        uint8_t type = static_cast<uint8_t>(messageType::chunkData);
        uint32_t chunk_id;
        uint32_t compressed_size;
        uint32_t original_size;
        uint32_t crc32;
        uint8_t compress_stra;
    };
#pragma pack(pop)
}
#endif //SXAINT_PROTOCOL_H