//
// Created by xint2 on 08/07/2026.
//

#ifndef SXAINT_METRICS_H
#define SXAINT_METRICS_H
#include <chrono>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace sxaint::core {
    class transferMetrics {
    public:
        transferMetrics(uint64_t total_bytes);
        void add_bytes(uint64_t bytes);
        double get_speed_mbps();
        uint32_t get_ets_secs();
        int get_progress_per() const;
        std::string get_ui_string();

    private:
        uint64_t total_bytes_;
        std::atomic<uint64_t> bytes_transferred_{0};
        std::chrono::steady_clock::time_point start_time_;
        std::chrono::steady_clock::time_point last_update_time_;
        uint64_t last_bytes_snap_{0};
        double current_speed_mbps_{0.0};
        uint32_t current_eta_secs_{0};
        std::mutex calc_mutex_; // prevent thread from trampling the clock calc
    };
}
#endif //SXAINT_METRICS_H