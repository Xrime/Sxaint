//
// Created by xint2 on 05/07/2026.
//

#ifndef SXAINT_PROTOCOL_H
#define SXAINT_PROTOCOL_H
#include <cstdint>

namespace sxaint::net {
#pragma pack(push, 1)

    enum class messageType: uint8_t {
        handshake = 1,
        chunkData = 2
    };
    struct handshake {
        messageType type{messageType::handshake};
        uint64_t file_size;
        uint32_t chunk_size;
        uint32_t total_chunks;
        char file_name[255];
    };
    struct chunkWireHeader {
        messageType type{messageType::chunkData};
        uint32_t chunk_id;
        uint32_t compressed_size;
        uint32_t original_size;
        uint32_t crc32;
        uint8_t compress_stra;


    };
#pragma pack(pop)
}
#endif //SXAINT_PROTOCOL_H