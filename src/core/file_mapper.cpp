//
// Created by xint2 on 03/07/2026.
//
#include "..//include/core/file_mapper.h"
#include <windows.h>
#include <spdlog/spdlog.h>


namespace sxaint::core {

    FileMapper::~FileMapper() {
        cleanup();
    }
    FileMapper::FileMapper(FileMapper &&other) noexcept {
        *this = std::move(other);
    }
    FileMapper &FileMapper::operator=(FileMapper &&other) noexcept {
        if (this != &other) {
            cleanup();
            fileHandle_ = other.fileHandle_;
            mappingHandle_ = other.mappingHandle_;
            mappedView_ = other.mappedView_;
            mappedLength_= other.mappedLength_;
            other.fileHandle_= INVALID_HANDLE_VALUE;
            other.mappingHandle_ = nullptr;
            other.mappedView_ = nullptr;
            other.mappedLength_ = 0;
        }
        return *this;
    }

    void FileMapper::cleanup() {
        if (mappedView_) {
            UnmapViewOfFile(mappedView_);
            mappedView_ = nullptr;
        }
        if (mappingHandle_) {
            CloseHandle(mappingHandle_);
            mappingHandle_ = nullptr;
        }
        if (fileHandle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(fileHandle_);
            fileHandle_ = INVALID_HANDLE_VALUE;
        }
        mappedLength_ = 0;

    }
    void FileMapper::preallocate_file(const std::filesystem::path &path, uint64_t size) {
        HANDLE hFile = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to create file for preallocation");
        }

        // --- ENTERPRISE FIX: SET SPARSE FLAG ---
        // Tells Windows NOT to write 6.5GB of zeroes to the physical disk.
        DWORD bytesReturned = 0;
        DeviceIoControl(hFile, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
        // ---------------------------------------

        LARGE_INTEGER lisize;
        lisize.QuadPart = size;
        SetFilePointerEx(hFile, lisize, nullptr, FILE_BEGIN);
        SetEndOfFile(hFile);
        CloseHandle(hFile);

        spdlog::info("Preallocated Sparse File {} with size {} bytes", path.filename().string(), size);
    }
    std::span<const std::byte> FileMapper::map_read(const std::filesystem::path &path, uint64_t offset, size_t length) {
        cleanup();
        fileHandle_ = CreateFileW(path.c_str(), GENERIC_READ,FILE_SHARE_READ,nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fileHandle_ == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open file for reading");
        mappingHandle_ = CreateFileMappingW(fileHandle_, nullptr, PAGE_READONLY,0,0, nullptr);
        if (!mappingHandle_) {
            throw std::runtime_error("failed to create file mapping");
        }
        ULARGE_INTEGER liOffset;
        liOffset.QuadPart = offset;

        mappedView_= MapViewOfFile(mappingHandle_,FILE_MAP_READ, liOffset.HighPart, liOffset.LowPart, length);
        if (!mappedView_) {
            throw std::runtime_error("Failed to map view of file");
        }
        mappedLength_ = length;
        return std::span<std::byte>(static_cast<std::byte*>(mappedView_),length);

    }
    std::span<std::byte> FileMapper::map_write(const std::filesystem::path &path, uint64_t offset, size_t length) {
        cleanup();
        fileHandle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fileHandle_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("failed to create file mapping");
        }
        mappingHandle_ = CreateFileMappingW(fileHandle_, nullptr, PAGE_READWRITE,0,0,nullptr);
        if (!mappingHandle_) {
            throw std::runtime_error("failed to create file mapping");
        }
        ULARGE_INTEGER liOffset;
        liOffset.QuadPart = offset;

        mappedView_ = MapViewOfFile(mappingHandle_, FILE_MAP_WRITE, liOffset.HighPart, liOffset.LowPart, length);
        if (!mappedView_) throw std::runtime_error("failed to map the view of file");
        mappedLength_ =length;
        return  std::span<std::byte>(static_cast<std::byte*>(mappedView_), length);
    }

    void FileMapper::unmap() {
        cleanup();
    }
    void FileMapper::advise_seq(std::span<const std::byte> region) {
        WIN32_MEMORY_RANGE_ENTRY entry;
        entry.VirtualAddress = const_cast<std::byte*>(region.data());
        entry.NumberOfBytes = region.size();
        //PrefetchVirtualMemory(GetCurrentProcess(),1,&entry,0);
    }








}