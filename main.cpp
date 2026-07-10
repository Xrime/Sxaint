#include "appwindow.h"
#include <complex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include "include/net/discovery.h"
#include <future>
#include <atomic>
#include "include/net/session.h"
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
using  namespace sxaint;
// void setup_logging() {
//     auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
//     console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
//     auto logger = std::make_shared<spdlog::logger>("sxaint", console_sink);
//     spdlog::set_default_logger(logger);
//     spdlog::set_level(spdlog::level::debug);
// }
// int main(int argc, char* argv[]) {
//     std::string mode;
//     std::filesystem::path target_path;
//     // setup_logging();
//
// #ifdef _WIN32
//     int wargc;
//     LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
//     if (wargc < 3) {
//         spdlog::error("Usage: Sxaint.exe send <file_path> OR receive <outut_dir>");
//         return 1;
//     }
//     std::wstring wmode = wargv[1];
//     mode = std::string(wmode.begin(), wmode.end());
//     target_path = std::filesystem::path(wargv[2]);
//     LocalFree(wargv);
// #else
//     if (argc<3) {
//         std::cerr <<"Usage:\n";
//         std::cerr <<"     Sender: Sxaint.exe receive <>output_directory>\n";
//         std::cerr <<"     Receiver: sxaint.exe send <file_path> \n";
//         return 1;
//     }
//     mode= argv[1];
//     target_path = std::filesystem::path(argv[2]);
// #endif
//
//     const uint16_t DATA_PORT = 9000;
//     const uint16_t DISCOVERY_PORT = 9001;
//
//     if (mode == "receive") {
//         net::Discovery discovery;
//         discovery.start(DISCOVERY_PORT, "Sxaint-Receiver");
//
//         net::Session session; //ready the kcp
//         session.recvFile(target_path, DATA_PORT);
//     }else if (mode == "send") {
//         net::Discovery discovery;
//         std::promise<std::string> peer_promise;
//         std::atomic<bool> found = false;
//
//         discovery.set_callback([&](const net::Peer& peer) {
//             if (peer.hostname == "Sxaint-Sender") return;
//
//            if (!found.exchange(true)) {
//                spdlog::info("found receiver '{}' at {}", peer.hostname, peer.ip_address);
//                peer_promise.set_value(peer.ip_address);
//            }
//         });
//         spdlog::info("Scanning local network for recv");
//         discovery.start(DISCOVERY_PORT, "Sxaint-Sender");
//         std::string target_ip ;
//         auto future = peer_promise.get_future();
//
//         if (future.wait_for(std::chrono::seconds(3)) == std::future_status::ready) {
//             target_ip = future.get();
//         } else {
//             spdlog::warn("No Wi-Fi peers found. Defaulting to local machine (127.0.0.1)");
//             target_ip = "127.0.0.1";
//         }
//         discovery.stop();
//         uint32_t user_pin = 0;
//         std::cout<<"\n Enter the 6-digit PIN displayed on the Receiver: ";
//         std::cin>> user_pin;
//         std::cout << "\n";
//         net::Session session;
//         session.sendFile(target_path, target_ip, DATA_PORT, user_pin);
//     }else {
//         spdlog::error("unknown mode: {}. use 'send' or 'recv'", mode);
//         return  1;
//     }
//     // try {
//     //     sxaint::net::Session session;
//     //     if (mode == "send" && __argc == 5) {
//     //         session.sendFile(__argv[2], __argv[3], static_cast<uint16_t>(std::stoi(__argv[4])));
//     //     }
//     //     else if (mode =="receive" && __argc == 4) {
//     //         session.recvFile(__argv[2], static_cast<uint16_t>(std::stoi(__argv[3])));
//     //     }
//     //     else {
//     //         spdlog::error("invalid argument");
//     //         return 1;
//     //     }
//     // }catch (const std::exception& e) {
//     //     spdlog::critical("Error: {} ", e.what());
//     //     return 1;
//     // }
//     return 0;
// }


std::string OpenWindowsFileDialog(bool selectFolder) {
    std::string outPath = "";
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED| COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog *pFileOpen;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (SUCCEEDED(hr)) {
            DWORD dwOptions;
            pFileOpen->GetOptions(&dwOptions);
            if (selectFolder) pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem *pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        std::wstring wpath(pszFilePath);
                        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wpath[0], (int)wpath.size(), NULL, 0, NULL,NULL);
                        outPath = std::string(sizeNeeded, 0);
                        WideCharToMultiByte(CP_UTF8,0,&wpath[0], (int)wpath.size(),&outPath[0], sizeNeeded, NULL, NULL);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }

            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return outPath;
}
int main(){
        auto ui = AppWindow::create();
        ui->on_start_trans([&ui] {
            bool isReceiver
            std::string mode = ui->get_mode().data();
            std::string filepath = ui->get_filepath().data();
            if (filepath.empty()) {
                ui->set_status_text("Error: Choose a valid path.");
                return;
            }
            if (mode == "send") {
                std::string pin_str = ui->get_pin_code().data();
                if (pin_str.length() != 6) {
                    ui->set_status_text("Error: PIN must be 6 digits");
                    return;
                }
                uint32_t pin =std::stoul(pin_str);
                ui->set_status_text("Searching for Receiver...");

                std::thread([&ui, filepath, pin]() {
                    net::Discovery discovery;
                    std::promise<std::string>peer_promise;
                    std::atomic<bool> found = false;
                    discovery.set_callback([&](const net::Peer& peer) {
                        if (peer.hostname == "Sxaint- Sender") return;
                        if (!found.exchange(true)) {
                            peer_promise.set_value(peer.ip_address);
                        }
                    });
                    discovery.start(9001,"Sxaint-Sender");
                    std::string target_ip;
                    auto future = peer_promise.get_future();

                    if (future.wait_for(std::chrono::seconds(3)) == std::future_status::ready) {
                        target_ip = future.get();
                    }else {
                        target_ip = "127.0.0.1";
                    }
                    discovery.stop();

                    slint::invoke_from_event_loop([&ui, target_ip]() {
                        ui->set_status_text("Connecting to " + slint::SharedString(target_ip) + "...");
                    });
                }).detach();
            }
            else if (mode== "Receive") {
                ui->set_status_text("Waiting for connections...");
                ui->set_pin_code("123456");
                std::thread([&ui, filepath]() {
                   net::Discovery discovery;
                    discovery.start(9001, "Sxaint- Receiver");
                    net::Session session;
                    session.recvFile(std::filesystem::path(filepath), 9000);
                    slint::invoke_from_event_loop([&ui]() {
                       ui->set_status_text("File received successfully");
                        ui->set_progress_value(1.0);
                    });

                }).detach();
            }
        });
    ui->run();
    return 0;
}
