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
#include <valarray>
#include "../include/net/stun.h"



using  namespace sxaint;
std::vector<std::string> OpenWindowsFileDialog(bool selectFolder) {
    // std::string outPath = "";
    std::vector<std::string> outPaths;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED| COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog *pFileOpen;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (SUCCEEDED(hr)) {
            DWORD dwOptions;
            pFileOpen->GetOptions(&dwOptions);
            if (selectFolder) {
                pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
            }else {
                pFileOpen->SetOptions(dwOptions | FOS_ALLOWMULTISELECT);
            }
            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItemArray *pItemArray;
                hr = pFileOpen->GetResults(&pItemArray);
                if (SUCCEEDED(hr)) {
                    DWORD count;
                    pItemArray->GetCount(&count);
                    for (DWORD i = 0; i < count; i++) {
                        IShellItem *pItem;
                        hr = pItemArray->GetItemAt(i, &pItem);
                        if(SUCCEEDED(hr)) {
                            PWSTR pszFilePath;
                            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                            if (SUCCEEDED(hr)) {
                                std::wstring wpath(pszFilePath);
                                int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wpath[0], (int)wpath.size(), NULL, 0, NULL,NULL);
                                std::string outPath(sizeNeeded, 0);
                                WideCharToMultiByte(CP_UTF8,0,&wpath[0], (int)wpath.size(),&outPath[0], sizeNeeded, NULL, NULL);
                                outPaths.push_back(outPath);
                                CoTaskMemFree(pszFilePath);
                            }
                            pItem->Release();
                        }
                    }

                    pItemArray->Release();
                }

            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return outPaths;
}
int main() {
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
        std::vector<std::string> selected_paths= OpenWindowsFileDialog(isReceiver);
        if (!selected_paths.empty()) {
            std::string joined_paths;
            for (size_t i = 0; i< selected_paths.size(); ++i) {
                joined_paths+= selected_paths[i];
                if (i < selected_paths.size() -1) joined_paths += "|";
            }
            ui->set_act_filepaths(slint::SharedString(joined_paths));
            // ui->set_filepath(slint::SharedString(selected_paths));
            if (selected_paths.size() == 1) {
                ui->set_filepath(slint::SharedString(selected_paths[0]));
            }else {
                ui->set_filepath(slint::SharedString(std::to_string(selected_paths.size())+ "files selected"));
            }
        }});
    ui->on_start_trans([&ui, log_history] {
        std::string mode = ui->get_mode().data();
        std::string raw_paths = ui->get_act_filepaths().data();
        if (raw_paths.empty()) {
           raw_paths= ui->get_filepath().data();
        }
        if (raw_paths.empty()) return;
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
        bool useInternet = ui->get_useInternet();
        std::string target_globalIP = ui->get_target_ip().data();

        if (mode == "Send") {

            std::string pin_str = ui->get_pin_code().data();
            if (pin_str.length() != 6) {
               ui->set_status_text("Error: PIN must be 6 digits");
               ui->set_is_active(false);
               return;
            }
            ui->set_status_text("Searching for Receiver...");
            std::vector<std::filesystem::path> file_queue;
            std::stringstream ss(raw_paths);
            std::string item;
            while (std::getline(ss,item, '|')) {
                if (!item.empty()) {
                   file_queue.push_back(std::filesystem::path(reinterpret_cast<const char8_t *>(item.c_str())));

               }
           }
            std::sort(file_queue.begin(), file_queue.end(),[](const std::filesystem::path& a, const std::filesystem::path& b) {
               std::error_code ec;
               return std::filesystem::file_size(a,ec)< std::filesystem::file_size(b,ec);
           });

            uint32_t pin =std::stoul(pin_str);
            if (useInternet && target_globalIP.empty()) {
                ui->set_status_text("Error : Enter the Receiver Global IP");
                ui->set_is_active(false);
                return;
            }
            ui->set_status_text(useInternet ? "Connecting to the Internet" : "Scanning local network");


            std::thread([ui_handle = ui, file_queue, pin, broadcastName, log_history, useInternet, target_globalIP]() {
                try {
                    std::string target_ip = target_globalIP;
                    if (!useInternet) {
                        net::Discovery discovery;
                        std::promise<std::string> peer_promise;
                        std::atomic<bool> found = false;

                        discovery.set_callback([&](const net::Peer& peer) {
                           if (peer.hostname == broadcastName) return;
                            if (!found.exchange(true)) peer_promise.set_value(peer.ip_address);

                        });
                        discovery.start(9001, broadcastName);
                        auto future = peer_promise.get_future();
                        if (future.wait_for(std::chrono::seconds(3)) ==std::future_status::ready) {
                            target_ip = future.get();
                        }else {
                            target_ip = "127.0.0.1";
                        }
                        discovery.stop();
                    }

                    int total_files = file_queue.size();
                    int current_file = 1;

                    // 4. Loop through the sorted queue and send sequentially!
                    for (const auto& path : file_queue) {
                        slint::invoke_from_event_loop([=]() {
                            ui_handle->set_status_text("Transferring to " + slint::SharedString(target_ip) + "...");
                            // Update the new UI Queue Text!
                            ui_handle->set_queue_text(slint::SharedString(fmt::format("Transferring: {} ({}/{})", path.filename().string(), current_file, total_files)));
                        });

                        net::Session session;
                        session.sendFile(path, target_ip, 9000, pin,
                            [ui_handle](int percent, double mbps, uint32_t eta) {
                                slint::invoke_from_event_loop([=]() {
                                    ui_handle->set_progress_value(percent / 100.0f);
                                    ui_handle->set_speed_text(slint::SharedString(fmt::format("{:.1f} MB/s", mbps)));
                                    ui_handle->set_eta_text(slint::SharedString(fmt::format("ETA: {}s", eta)));
                                });
                            });

                        log_history(path.string(), "Sent", "Success");
                        current_file++;

                        // Give the Receiver 500ms to reset its sockets before blasting the next file
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }

                    slint::invoke_from_event_loop([=]() {
                        ui_handle->set_status_text("All files transferred successfully!");
                        ui_handle->set_queue_text("");
                        ui_handle->set_is_active(false);
                    });

                } catch (const std::exception& e) {
                    spdlog::error("FATAL ERROR: {}", e.what());
                    slint::invoke_from_event_loop([=]() {
                        ui_handle->set_status_text("Error: Transfer failed. Check console.");
                        ui_handle->set_is_active(false);
                    });
                }
            }).detach();

        // ==========================================
        // RECEIVER MODE (CONTINUOUS LOOP)
        // ==========================================
        } else if (mode == "Receive") {

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> distrib(100000, 999999);
            uint32_t secure_pin = distrib(gen);

            ui->set_pin_code(slint::SharedString(std::to_string(secure_pin)));
            ui->set_status_text("Listening for connections...");
            ui->set_status_text("Resolving network status");
            std::thread([ui_handle = ui, target_dir = raw_paths, secure_pin, broadcastName, log_history, useInternet]() {
                try {
                    if (useInternet) {
                        auto pub_endpoint =  net::stunClient::getPublicEndpoint(9000);
                        slint::invoke_from_event_loop([=]() {
                            if (pub_endpoint.success) {
                                ui_handle->set_status_text(slint::SharedString("Global IP: " + pub_endpoint.ip));
                            }else {
                                ui_handle->set_status_text("Failed to get Global IP.");
                            }
                        });

                    }else {
                        net::Discovery discovery;
                        discovery.start(9001, broadcastName);
                        slint::invoke_from_event_loop([=]() {
                           ui_handle->set_status_text("Listening on Local Network..");
                        });
                    }
                    net::Discovery discovery;
                    discovery.start(9001, broadcastName);

                    int files_received = 0;

                    // 5. Infinite loop to keep receiving queued files one after another!
                    while (true) {
                        slint::invoke_from_event_loop([=]() {
                            if (files_received > 0) {
                                ui_handle->set_queue_text(slint::SharedString(fmt::format("Files Received: {}", files_received)));
                            }
                        });

                        net::Session session;
                        uint16_t listenPort = useInternet ? 9002 : 9000;
                        session.recvFile(std::filesystem::path(reinterpret_cast<const char8_t*>(target_dir.c_str())), listenPort    , secure_pin,
                            [ui_handle](int percent, double mbps, uint32_t eta) {
                                slint::invoke_from_event_loop([=]() {
                                    ui_handle->set_progress_value(percent / 100.0f);
                                    ui_handle->set_speed_text(slint::SharedString(fmt::format("{:.1f} MB/s", mbps)));
                                    ui_handle->set_eta_text(slint::SharedString(fmt::format("ETA: {}s", eta)));
                                });
                            });

                        files_received++;
                        log_history(target_dir, "Received", "Success");

                        slint::invoke_from_event_loop([=]() {
                            ui_handle->set_status_text("Listening for next file...");
                        });

                        // Buffer wait to avoid port collision
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                } catch (const std::exception& e) {
                    spdlog::error("FATAL ERROR: {}", e.what());
                    slint::invoke_from_event_loop([=]() {
                        ui_handle->set_status_text("Error: Receive failed.");
                        ui_handle->set_is_active(false);
                    });
                }
            }).detach();
        }
    });
    ui->run();
    return 0;
}
