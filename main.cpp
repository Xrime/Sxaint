#include <complex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

#include "include/net/session.h"

void setup_logging() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    auto logger = std::make_shared<spdlog::logger>("sxaint", console_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
}
int main() {
    setup_logging();

    if (__argc<2) {
        std::cerr <<"Usage:\n";
        std::cerr <<"     Sender: sxaint send <file_path> <target_ip> <port>\n";
        std::cerr <<"     Receiver: sxaint receive <output_dir> <port>\n";
        return 1;
    }
    std::string mode = __argv[1];
    try {
        sxaint::net::Session session;
        if (mode == "send" && __argc == 5) {
            session.sendFile(__argv[2], __argv[3], static_cast<uint16_t>(std::stoi(__argv[4])));
        }
        else if (mode =="receive" && __argc == 4) {
            session.recvFile(__argv[2], static_cast<uint16_t>(std::stoi(__argv[3])));
        }
        else {
            spdlog::error("invalid argument");
            return 1;
        }
    }catch (const std::exception& e) {
        spdlog::critical("Error: {} ", e.what());
        return 1;
    }
    return 0;
}