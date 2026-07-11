#include "appwindow.h"
#include <complex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include "include/net/discovery.h"
#include <future>
#include <atomic>
#include <random>

#include "include/net/session.h"
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <Lmcons.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>


using  namespace sxaint;
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
    auto history_model = std::make_shared<slint::VectorModel<historyRecord>>();
    ui->set_history_model(history_model);

    std::ifstream ifs("sxaint_history.txt");
    std::string line;
    std::vector<historyRecord> loaded_records;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string file, dir,status,time;

        if (std::getline(iss,file,'|') && std::getline(iss, dir, '|') &&
            std::getline(iss, status, '|') && std::getline(iss, time)) {
            historyRecord rec;
            rec.filename = slint::SharedString(file);
            rec.direction = slint::SharedString(dir);
            rec.status = slint::SharedString(status);
            rec.timestamp = slint::SharedString(time);
            loaded_records.insert(loaded_records.begin(),rec);
        }
    }
    for (const auto& rec: loaded_records) {
        history_model->push_back(rec);
    }
    auto log_history = [history_model](const std::string& filepath, const std::string& dir,const std::string& status) {
        std::string filename = std::filesystem::path(reinterpret_cast<const char8_t *>(filepath.c_str())).filename().string();
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time (std::localtime(&now_c), "%Y-%m-%d %H:%M");
        std::string time_str = ss.str();

        std::ofstream ofs("sxaint_history.txt", std::ios::app);
        if (ofs.is_open()) {
            ofs<< filename<< "|"<< dir<< "|" <<status<<"|"<<time_str<< "\n";

        }
        slint::invoke_from_event_loop([=]() {
            historyRecord rec;
            rec.filename =slint::SharedString(filename);
            rec.direction = slint::SharedString(dir);
            rec.status = slint::SharedString(status);
            rec.timestamp = slint::SharedString(time_str);
            history_model->insert(0, rec);
        });
    };
    ui->on_browse_file([&ui] {
        std::string mode = ui->get_mode().data();
        bool isReceiver =(mode == "Receive");
        std::string selected_path= OpenWindowsFileDialog(isReceiver);
        if (!selected_path.empty()) {
            ui->set_filepath(slint::SharedString(selected_path));
        }});
    ui->on_start_trans([&ui, log_history] {
        std::string mode = ui->get_mode().data();
        std::string filepath = ui->get_filepath().data();
        if (filepath.empty()) {
            ui->set_status_text("Error: Choose a valid path.");
            return;
        }
        ui->set_is_active(true);
        // auto log_to_ui = [ui_handle = ui](const std::string& msg) {
        //     spdlog::info(msg);
        //     slint::invoke_from_event_loop([=]() {
        //        auto current = ui_handle->get_transfer_log().data();
        //         std::string new_log = std::string(current) + msg+"\n";
        //         ui_handle->set_transfer_log(slint::SharedString(new_log));
        //     });
        // };
        char username[UNLEN + 1];
        DWORD usernameLen = UNLEN +1;
        GetUserNameA(username, &usernameLen);
        std::string broadcastName = std::string(username) + "'s PC";

        if (mode == "Send") {
            std::string pin_str = ui->get_pin_code().data();
            if (pin_str.length() != 6) {
                ui->set_status_text("Error: PIN must be 6 digits");
                ui->set_is_active(false);
                return;
            }
            uint32_t pin =std::stoul(pin_str);
            ui->set_status_text("Searching for Receiver...");

            std::thread([ui_handle = ui, filepath, pin, broadcastName, log_history]() {
                try {
                    net::Discovery discovery;
                    std::promise<std::string>peer_promise;
                    std::atomic<bool> found = false;
                    discovery.set_callback([&](const net::Peer& peer) {
                        if (peer.hostname == broadcastName) return;
                        if (!found.exchange(true)) {
                            peer_promise.set_value(peer.ip_address);
                        }
                    });
                    discovery.start(9001, broadcastName);
                    std::string target_ip;
                    auto future = peer_promise.get_future();

                    if (future.wait_for(std::chrono::seconds(3)) == std::future_status::ready) {
                        target_ip = future.get();
                    }else {
                        target_ip = "127.0.0.1";
                    }
                    discovery.stop();

                    slint::invoke_from_event_loop([=]() {
                        ui_handle->set_status_text("Connecting to " + slint::SharedString(target_ip) + "...");
                    });

                    net::Session session;
                    session.sendFile(std::filesystem::path(reinterpret_cast<const char8_t*>(filepath.c_str())), target_ip,9000, pin,
                        [ui_handle](int percent, double mbps,uint32_t eta) {
                        slint::invoke_from_event_loop([=]() {
                            ui_handle->set_progress_value(percent / 100.0f);
                            ui_handle->set_speed_text(slint::SharedString(fmt::format("{:.1f} MB/s", mbps)));
                            ui_handle->set_eta_text(slint::SharedString(fmt::format("ETA: {}s",eta)));
                        });
                    });
                    slint::invoke_from_event_loop([=]() {
                       ui_handle->set_status_text("Transfer Complete");
                        ui_handle->set_is_active(false);
                    });
                    log_history(filepath, "Sent", "Success");
                }catch (const std::exception& e) {
                    spdlog::error("Fatal Error: {}", e.what());
                    slint::invoke_from_event_loop([=]() {
                        ui_handle->set_status_text("Error: Transfer failed.");
                        ui_handle->set_is_active(false);
                    });
                    log_history(filepath, "Sent", "Failed");
                }
            }).detach();
        }
        else if (mode== "Receive") {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> distrib(100000,999999);
            uint32_t secure_pin = distrib(gen);
            ui->set_pin_code(slint::SharedString(std::to_string(secure_pin)));
            ui->set_status_text("Waiting for connections...");

            std::thread([ui_handle =ui, filepath, secure_pin, broadcastName, log_history]() {
               net::Discovery discovery;
                discovery.start(9001, "Sxaint-Receiver");
                net::Session session;
                session.recvFile(std::filesystem::path(reinterpret_cast<const char8_t*>(filepath.c_str())), 9000, secure_pin,[ui_handle](int percent, double mbps, uint32_t eta) {
                    slint::invoke_from_event_loop([=]() {
                        ui_handle->set_progress_value(percent / 100.0f);
                        ui_handle->set_speed_text(slint::SharedString(fmt::format("{:.1f} MB/s", mbps)));
                        ui_handle->set_eta_text(slint::SharedString(fmt::format("ETA: {}s", eta)));
                    });
                });
                slint::invoke_from_event_loop([=]() {
                    ui_handle->set_status_text("File received successfully");
                    ui_handle->set_progress_value(1.0f);
                    ui_handle->set_is_active(false);
                });
                log_history(filepath, "Received", "Success");

            }).detach();
        }
    });
    ui->run();
    return 0;
}
