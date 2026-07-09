//
// Created by xint2 on 08/07/2026.
//
#include "../../include/core/metrics.h"
#include <spdlog/fmt/fmt.h>
#include <algorithm>
#include <mutex>

namespace sxaint::core {
    transferMetrics::transferMetrics(uint64_t total_bytes) : total_bytes_(total_bytes){
        start_time_ = std::chrono::steady_clock::now();
        last_update_time_ = start_time_;
    }
    void transferMetrics::add_bytes(uint64_t bytes) {
        bytes_transferred_.fetch_add(bytes,std::memory_order_relaxed);

    }
    double transferMetrics::get_speed_mbps() {
        std::lock_guard<std::mutex> lock(calc_mutex_);
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - last_update_time_;

        if (elapsed.count() >= 0.25) {
            uint64_t current_bytes = bytes_transferred_.load(std::memory_order_relaxed);
            uint64_t bytes_since_last = current_bytes - last_bytes_snap_;

            current_speed_mbps_ = (static_cast<double>(bytes_since_last) / 1048576.0)/elapsed.count();
            uint64_t remaining_bytes = (total_bytes_ > current_bytes)? total_bytes_ - current_bytes:0;
            if (current_speed_mbps_ > 0.1) {
                current_eta_secs_ = static_cast<uint32_t>((remaining_bytes / 1048576.0) / current_speed_mbps_);
            }else {
                current_eta_secs_ = 999;
            }
            last_bytes_snap_ = current_bytes;
            last_update_time_ = now;
        }
        return current_speed_mbps_;
    }
    uint32_t transferMetrics::get_ets_secs() {
        get_speed_mbps();
        return current_eta_secs_;
    }
    int transferMetrics::get_progress_per() const {
        if (total_bytes_ == 0) return 100;
        return static_cast<int>((bytes_transferred_.load(std::memory_order_relaxed) * 100)/total_bytes_);
    }
    std::string transferMetrics::get_ui_string() {
        int percent = get_progress_per();
        double speed =get_speed_mbps();
        uint32_t eta = get_ets_secs();

        int bars = (percent* 40) / 100;
        std::string bar = std::string(bars, '=') + std::string(40 - bars, ' ');
        return fmt::format("[{}] {}% | {:.1f} MB/s | ETA: {}",bar , percent,speed,eta);

    }






}
