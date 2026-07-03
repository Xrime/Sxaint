//
// Created by xint2 on 03/07/2026.
//

#ifndef SXAINT_FILE_MAPPER_H
#define SXAINT_FILE_MAPPER_H
#include <filesystem>
#include <span>
#include <cstddef>
#include <stdexcept>
#include <windows.h>

namespace sxaint::core {
    class FileMapper {
    public:
        FileMapper() = default;
        ~FileMapper();
        FileMapper(const FileMapper&) = delete;
        FileMapper& operator= (const FileMapper&) = delete;
        FileMapper(FileMapper&& other) noexcept;
        FileMapper& operator = (FileMapper&& other) noexcept;
        static  void preallocate_file(const std::filesystem::path& path,uint64_t size);
        std::span<const std::byte> map_read(const std::filesystem::path& path, uint64_t offset, size_t length);
        std::span<std::byte> map_write(const std::filesystem::path& path,uint64_t offset, size_t length);
        void unmap();
        void advise_seq(std::span<const std::byte> region);
    private:
        void cleanup();
        HANDLE fileHandle_{INVALID_HANDLE_VALUE};
        HANDLE mappingHandle_{nullptr};
        void* mappedView_{nullptr};
        size_t mappedLength_{0};

    };
}
#endif //SXAINT_FILE_MAPPER_H