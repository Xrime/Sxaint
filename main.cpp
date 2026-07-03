#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
void setup_logging() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    auto logger = std::make_shared<spdlog::logger>("sxaint", console_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
}
int main() {
    setup_logging();
    spdlog::info("SXAINT Initialization Started...");
    spdlog::info("Target OS: Windows(Win32 API)");
    spdlog::info("SXAINT Shutdown Complete. ");
    return 0;
}