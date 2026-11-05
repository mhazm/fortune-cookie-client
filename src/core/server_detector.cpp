#include "core/server_detector.hpp"
#include "core/telemetry_state.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace nismara::core {

namespace {

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) {
        return {};
    }
    const int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(size_needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string TrimWhitespace(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string ReadFileTailShared(const std::filesystem::path& path, size_t max_bytes = 49152) {
    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return {};
    }

    LARGE_INTEGER file_size{};
    if (!GetFileSizeEx(hFile, &file_size) || file_size.QuadPart <= 0) {
        CloseHandle(hFile);
        return {};
    }

    const LONGLONG total_size = file_size.QuadPart;
    const LONGLONG read_size = (total_size > static_cast<LONGLONG>(max_bytes))
                                   ? static_cast<LONGLONG>(max_bytes)
                                   : total_size;
    LARGE_INTEGER seek_pos{};
    seek_pos.QuadPart = total_size - read_size;
    SetFilePointerEx(hFile, seek_pos, nullptr, FILE_BEGIN);

    std::string buffer(static_cast<size_t>(read_size), '\0');
    DWORD bytes_read = 0;
    ReadFile(hFile, buffer.data(), static_cast<DWORD>(read_size), &bytes_read, nullptr);
    CloseHandle(hFile);

    buffer.resize(bytes_read);
    return buffer;
}

bool IsStandardIgnoredModule(const std::string& lower_name, const std::string& lower_path) {
    if (lower_path.find("\\windows\\") != std::string::npos) {
        return true;
    }
    static const char* kStandardModules[] = {
        "eurotrucks2.exe",
        "amtrucks.exe",
        "nismaratracker.dll",
        "fmod.dll",
        "fmodstudio.dll",
        "steam_api64.dll",
        "d3dcompiler_47.dll",
        "tobii.gameintegration.x64.dll",
        "tobii_gameintegration_x64.dll",
        "gameoverlayrenderer64.dll",
        "steamclient64.dll",
        "tier0_s64.dll",
        "vstdlib_s64.dll",
        "vulkan-1.dll",
        "dxgi.dll",
        "d3d11.dll",
        "dstorage.dll",
        "dstoragecore.dll",
        "hidapi.dll",
        "thrustmaster_bridge_x64.dll"
    };
    for (const char* std_mod : kStandardModules) {
        if (lower_name == std_mod) {
            return true;
        }
    }
    return false;
}

struct NetworkInspectResult {
    int game_tcp_established{0};
    int game_udp_sockets{0};
    std::string remote_endpoints{};
    std::string summary{};
};

NetworkInspectResult InspectProcessNetworkSockets() {
    NetworkInspectResult res{};
    const DWORD current_pid = GetCurrentProcessId();
    std::vector<std::string> endpoints;

    // 1. Inspect IPv4 TCP Table for current game process
    DWORD tcp_size = 0;
    GetExtendedTcpTable(nullptr, &tcp_size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (tcp_size > 0) {
        std::vector<BYTE> tcp_buf(tcp_size);
        if (GetExtendedTcpTable(tcp_buf.data(), &tcp_size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(tcp_buf.data());
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                const auto& row = table->table[i];
                if (row.dwOwningPid != current_pid || row.dwState != MIB_TCP_STATE_ESTAB) {
                    continue;
                }
                const uint32_t remote_ip_host = ntohl(row.dwRemoteAddr);
                const uint16_t remote_port = ntohs(static_cast<u_short>(row.dwRemotePort));
                // Ignore loopback (127.x.x.x), 0.0.0.0, and standard HTTP/HTTPS web calls (80, 443, 3000)
                if (remote_ip_host == 0 || (remote_ip_host >> 24) == 127 ||
                    remote_port == 80 || remote_port == 443 || remote_port == 3000) {
                    continue;
                }

                ++res.game_tcp_established;
                if (endpoints.size() < 2) {
                    IN_ADDR addr{};
                    addr.S_un.S_addr = row.dwRemoteAddr;
                    char ip_str[INET_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
                    char ep[64];
                    std::snprintf(ep, sizeof(ep), "%s:%u", ip_str, static_cast<unsigned>(remote_port));
                    endpoints.emplace_back(ep);
                }
            }
        }
    }

    // 2. Inspect IPv4 UDP Table for current game process
    DWORD udp_size = 0;
    GetExtendedUdpTable(nullptr, &udp_size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (udp_size > 0) {
        std::vector<BYTE> udp_buf(udp_size);
        if (GetExtendedUdpTable(udp_buf.data(), &udp_size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
            auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(udp_buf.data());
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                const auto& row = table->table[i];
                if (row.dwOwningPid != current_pid) {
                    continue;
                }
                const uint16_t local_port = ntohs(static_cast<u_short>(row.dwLocalPort));
                if (local_port > 0) {
                    ++res.game_udp_sockets;
                }
            }
        }
    }

    char sum_buf[128];
    if (!endpoints.empty()) {
        std::string ep_joined = endpoints[0];
        if (endpoints.size() > 1) {
            ep_joined += ", " + endpoints[1];
        }
        res.remote_endpoints = ep_joined;
        std::snprintf(
            sum_buf,
            sizeof(sum_buf),
            "TCP: %d (%s) | UDP: %d",
            res.game_tcp_established,
            ep_joined.c_str(),
            res.game_udp_sockets);
    } else {
        std::snprintf(
            sum_buf,
            sizeof(sum_buf),
            "Game TCP: %d | Active UDP Ports: %d",
            res.game_tcp_established,
            res.game_udp_sockets);
    }
    res.summary = sum_buf;
    return res;
}

} // namespace

ServerDetector& ServerDetector::Instance() {
    static ServerDetector instance;
    return instance;
}

void ServerDetector::Initialize() {
    if (running_.exchange(true)) {
        return;
    }
    worker_thread_ = std::thread(&ServerDetector::WorkerLoop, this);
}

void ServerDetector::Shutdown() {
    if (!running_.exchange(false)) {
        return;
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void ServerDetector::RequestImmediateScan() {
    force_scan_.store(true);
}

void ServerDetector::WorkerLoop() {
    PerformScan();

    int elapsed_ticks = 0;
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (!running_.load()) {
            break;
        }
        ++elapsed_ticks;
        if (force_scan_.exchange(false) || elapsed_ticks >= 10) { // Every 2.5 seconds
            elapsed_ticks = 0;
            PerformScan();
        }
    }
}

void ServerDetector::PerformScan() {
    bool has_truckersmp_dll = false;
    bool has_haulmp_dll = false;
    bool has_steam_net_dll = false;
    std::string detected_tmp_dll_name{};
    std::string detected_haulmp_dll_name{};
    std::vector<std::string> custom_modules;

    // 1. Scan Loaded Process Modules (DLLs) — Most Authoritative In-Process Signal
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (Module32FirstW(hSnap, &me)) {
            do {
                const std::string mod_name = WideToUtf8(me.szModule);
                const std::string mod_path = WideToUtf8(me.szExePath);
                const std::string lower_name = ToLowerAscii(mod_name);
                const std::string lower_path = ToLowerAscii(mod_path);

                // TruckersMP injects core_ets2mp.dll or core_atsmp.dll from TruckersMP directory
                if (lower_name.find("core_ets2mp") != std::string::npos ||
                    lower_name.find("core_atsmp") != std::string::npos ||
                    lower_path.find("\\truckersmp\\") != std::string::npos ||
                    lower_path.find("\\ets2mp\\") != std::string::npos ||
                    lower_path.find("\\atsmp\\") != std::string::npos) {
                    has_truckersmp_dll = true;
                    detected_tmp_dll_name = mod_name;
                }

                // HaulMP injects haulmp_stub.dll (from AppData\Local\HaulMP) and haulmp-telemetry.dll
                if (lower_name == "haulmp_stub.dll" ||
                    lower_name.find("haulmp") != std::string::npos ||
                    lower_path.find("\\haulmp\\") != std::string::npos) {
                    has_haulmp_dll = true;
                    if (detected_haulmp_dll_name.empty() || lower_name == "haulmp_stub.dll") {
                        detected_haulmp_dll_name = mod_name;
                    }
                }

                if (lower_name == "steamnetworkingsockets.dll") {
                    has_steam_net_dll = true;
                }

                if (!IsStandardIgnoredModule(lower_name, lower_path)) {
                    custom_modules.push_back(mod_name);
                }
            } while (Module32NextW(hSnap, &me));
        }
        CloseHandle(hSnap);
    }

    // 2. Inspect Process Command Line
    const wchar_t* raw_cmd = GetCommandLineW();
    const std::string cmdline = (raw_cmd != nullptr) ? WideToUtf8(raw_cmd) : "";
    const std::string lower_cmd = ToLowerAscii(cmdline);

    const bool cmd_has_truckersmp =
        (lower_cmd.find("truckersmp") != std::string::npos ||
         lower_cmd.find("ets2mp") != std::string::npos ||
         lower_cmd.find("atsmp") != std::string::npos);
    const bool cmd_has_haulmp =
        (lower_cmd.find("haulmp") != std::string::npos);

    // 3. Inspect Game Log (`game.log.txt`)
    // IMPORTANT: Avoid false positives like `[TRUCKY] Is TruckersMP: 0`!
    const auto snap = TelemetryState::Instance().GetSnapshot();
    const bool is_ats = (snap.game_id == "ats");

    wchar_t docs_w[MAX_PATH] = {0};
    std::filesystem::path docs_dir;
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs_w))) {
        docs_dir = docs_w;
    }

    bool convoy_session_active = false;
    bool physx_mp_server_running = false;
    bool log_mounts_haulmp = false;
    bool log_trucky_says_tmp = false;
    std::string last_mp_log_line = "No [mp] log entry found";

    if (!docs_dir.empty()) {
        const std::filesystem::path game_log_path =
            docs_dir / (is_ats ? L"American Truck Simulator" : L"Euro Truck Simulator 2") / L"game.log.txt";
        const std::string log_tail = ReadFileTailShared(game_log_path, 65536);
        if (!log_tail.empty()) {
            std::istringstream iss(log_tail);
            std::string line;
            std::vector<std::string> lines;
            while (std::getline(iss, line)) {
                lines.push_back(TrimWhitespace(line));
            }

            bool decided_convoy_state = false;
            bool decided_physx_state = false;

            for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                if (it->empty()) {
                    continue;
                }
                const std::string low = ToLowerAscii(*it);

                // Check HaulMP specific archive mounts or localization modules in game.log.txt
                if (low.find("local_module.haulmp.sii") != std::string::npos ||
                    low.find("haulmp_ui.scs") != std::string::npos ||
                    low.find("haulmp_cb99.scs") != std::string::npos ||
                    low.find("/haulmp/skins/") != std::string::npos) {
                    log_mounts_haulmp = true;
                    if (last_mp_log_line == "No [mp] log entry found") {
                        last_mp_log_line = *it;
                    }
                }

                // Check Trucky explicit flag ONLY if it says `: 1` (never `: 0`!)
                if (low.find("[trucky] is truckersmp: 1") != std::string::npos) {
                    log_trucky_says_tmp = true;
                }

                // Track `[physx] @0: Starting server.` vs `Stopping server.` (used by Convoy & HaulMP sessions)
                if (!decided_physx_state) {
                    if (low.find("[physx] @0: stopping server") != std::string::npos ||
                        low.find("[bullet] stopping server") != std::string::npos) {
                        physx_mp_server_running = false;
                        decided_physx_state = true;
                    } else if (low.find("[physx] @0: starting server") != std::string::npos ||
                               low.find("[bullet] starting server") != std::string::npos) {
                        physx_mp_server_running = true;
                        decided_physx_state = true;
                        if (last_mp_log_line == "No [mp] log entry found") {
                            last_mp_log_line = *it;
                        }
                    }
                }

                // Check SCS Official Convoy `[mp]` log entries
                if (low.find("[mp]") != std::string::npos) {
                    if (last_mp_log_line == "No [mp] log entry found") {
                        last_mp_log_line = *it;
                    }
                    if (!decided_convoy_state) {
                        if (low.find("session ended") != std::string::npos ||
                            low.find("disconnected") != std::string::npos ||
                            low.find("leaving") != std::string::npos ||
                            low.find("shutdown") != std::string::npos ||
                            low.find("closed") != std::string::npos ||
                            low.find("left") != std::string::npos) {
                            convoy_session_active = false;
                            decided_convoy_state = true;
                        } else if (low.find("session") != std::string::npos ||
                                   low.find("connected") != std::string::npos ||
                                   low.find("joining") != std::string::npos ||
                                   low.find("lobby") != std::string::npos ||
                                   low.find("server") != std::string::npos ||
                                   low.find("host") != std::string::npos ||
                                   low.find("player") != std::string::npos) {
                            convoy_session_active = true;
                            decided_convoy_state = true;
                        }
                    }
                }
            }
        }
    }

    // 4. Inspect Active Process TCP/UDP Sockets
    const NetworkInspectResult net_info = InspectProcessNetworkSockets();

    // 5. Read TruckersMP Server Name ONLY when TruckersMP DLL is actually loaded in this process!
    std::string tmp_server_name = "TruckersMP Network";
    if ((has_truckersmp_dll || cmd_has_truckersmp || log_trucky_says_tmp) && !docs_dir.empty()) {
        const std::filesystem::path tmp_log_dir = docs_dir / (is_ats ? L"ATSMP" : L"ETS2MP") / L"logs";
        std::error_code ec;
        if (std::filesystem::exists(tmp_log_dir, ec)) {
            std::filesystem::path newest_log;
            std::filesystem::file_time_type newest_time{};
            for (const auto& entry : std::filesystem::directory_iterator(tmp_log_dir, ec)) {
                if (entry.is_regular_file()) {
                    const auto ftime = entry.last_write_time(ec);
                    if (newest_log.empty() || ftime > newest_time) {
                        newest_time = ftime;
                        newest_log = entry.path();
                    }
                }
            }
            if (!newest_log.empty()) {
                const std::string tmp_tail = ReadFileTailShared(newest_log, 32768);
                std::istringstream iss(tmp_tail);
                std::string line;
                while (std::getline(iss, line)) {
                    const std::string low = ToLowerAscii(line);
                    if (low.find("connecting to") != std::string::npos ||
                        low.find("connected to") != std::string::npos) {
                        std::string cleaned = TrimWhitespace(line);
                        // Strip leading timestamp "[hh:mm:ss] " if present
                        const auto close_bracket = cleaned.find("] ");
                        if (close_bracket != std::string::npos && close_bracket < 14) {
                            cleaned = cleaned.substr(close_bracket + 2);
                        }
                        tmp_server_name = cleaned;
                    }
                }
            }
        }
    }

    // 6. Authoritative Priority Classification:
    //    Priority 1: HaulMP (haulmp_stub.dll / haulmp_ui.scs mounted)
    //    Priority 2: TruckersMP (core_ets2mp.dll / core_atsmp.dll)
    //    Priority 3: SCS Official Convoy ([mp] session or vanilla PhysX server without HaulMP)
    //    Priority 4: Singleplayer
    std::string final_mode = "Singleplayer";
    std::string final_name = "Offline / Standard Career";
    std::string final_source = "No Multiplayer Hooks Active";

    if (has_haulmp_dll || cmd_has_haulmp || log_mounts_haulmp) {
        final_mode = "HaulMP";
        final_name = "HaulMP";
        if (!net_info.remote_endpoints.empty()) {
            final_name += " [" + net_info.remote_endpoints + "]";
        }
        if (has_haulmp_dll) {
            final_source = "DLL Injected: " + detected_haulmp_dll_name;
        } else if (log_mounts_haulmp) {
            final_source = "Mounted: haulmp_ui.scs / haulmp.sii";
        } else {
            final_source = "Launch Args (HaulMP)";
        }
    } else if (has_truckersmp_dll || cmd_has_truckersmp || log_trucky_says_tmp) {
        final_mode = "TruckersMP";
        final_name = tmp_server_name;
        if (!net_info.remote_endpoints.empty()) {
            final_name += " [" + net_info.remote_endpoints + "]";
        }
        if (has_truckersmp_dll) {
            final_source = "DLL Injected: " + detected_tmp_dll_name;
        } else {
            final_source = "TruckersMP Client Hook";
        }
    } else if (convoy_session_active || physx_mp_server_running) {
        final_mode = "SCS Convoy";
        final_name = "Official Convoy Multiplayer";
        final_source = convoy_session_active ? "game.log [mp] Session" : "PhysX Convoy Server Active";
    }

    // Format Custom Modules list for the Debug Widget
    std::string modules_str;
    if (custom_modules.empty()) {
        modules_str = "None (Clean Vanilla Process)";
    } else {
        for (size_t i = 0; i < custom_modules.size(); ++i) {
            if (i > 0) {
                modules_str += ", ";
            }
            modules_str += custom_modules[i];
        }
    }

    TelemetryState::Instance().Write([&](TelemetrySnapshot& s) {
        s.server_mode = final_mode;
        s.server_name = final_name;
        s.server_detect_source = final_source;
        s.server_debug_modules = modules_str;
        s.server_debug_net_summary = net_info.summary;
        s.server_debug_last_log = last_mp_log_line;
        s.server_debug_cmdline = cmdline;
    });
}

} // namespace nismara::core
