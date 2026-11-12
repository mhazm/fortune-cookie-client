#include "core/api_client.hpp"
#include "core/config_manager.hpp"
#include "core/telemetry_state.hpp"
#include "ui/texture_manager.hpp"

#include <windows.h>
#include <winhttp.h>
#include "json.hpp"

#include <ctime>
#include <fstream>
#include <vector>

namespace nismara::core {

namespace {

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }
    const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (size_needed <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(size_needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size_needed);
    return result;
}

std::string GetLocalTimeString() {
    std::time_t now = std::time(nullptr);
    std::tm local_tm = {};
    localtime_s(&local_tm, &now);
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &local_tm);
    return buf;
}

void LogDebugToFile(const std::string& msg) {
    try {
        const auto dir = ConfigManager::Instance().GetGameUserDataDirectory();
        if (dir.empty()) return;
        std::ofstream ofs(dir / "nismara_tracker_debug.log", std::ios::app);
        if (ofs.is_open()) {
            ofs << "[" << GetLocalTimeString() << "] " << msg << "\n";
        }
    } catch (...) {}
}

std::string GuessExtensionFromUrl(const std::string& url) {
    const auto qpos = url.find('?');
    const std::string clean = (qpos == std::string::npos) ? url : url.substr(0, qpos);
    const auto dot = clean.find_last_of('.');
    if (dot != std::string::npos && clean.size() - dot <= 6) {
        std::string ext = clean.substr(dot);
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".bmp" || ext == ".gif") {
            return ext;
        }
    }
    return ".webp";
}

} // namespace

ApiClient& ApiClient::Instance() {
    static ApiClient instance;
    return instance;
}

void ApiClient::SetWallpaperStatus(const std::string& msg) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    wallpaper_status_ = msg;
}

std::string ApiClient::GetWallpaperDownloadStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return wallpaper_status_;
}

void ApiClient::SyncAccountAsync() {
    if (shutting_down_.load() || is_syncing_.exchange(true)) {
        return;
    }

    const auto cfg = ConfigManager::Instance().GetConfig();
    const auto snap = TelemetryState::Instance().GetSnapshot();

    if (cfg.driver_token.empty()) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::InvalidToken;
            acc.status_message = "Driver Token is empty. Paste your token from Dashboard Settings.";
        });
        is_syncing_.store(false);
        return;
    }

    TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
        acc.status = AccountSyncStatus::Connecting;
        acc.status_message = "Menyinkronkan dengan Nismara Cloud...";
    });

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    worker_thread_ = std::thread(
        &ApiClient::ExecuteSyncWorker,
        this,
        cfg.api_base_url,
        cfg.driver_token,
        cfg.steam_id,
        snap.game_id);
}

void ApiClient::DownloadWallpaperFromUrlAsync(
    const std::string& url,
    bool apply_lockscreen,
    bool apply_homescreen) {
    if (shutting_down_.load() || is_downloading_wallpaper_.exchange(true)) {
        return;
    }

    if (url.empty()) {
        SetWallpaperStatus("Wallpaper URL is empty.");
        is_downloading_wallpaper_.store(false);
        return;
    }

    SetWallpaperStatus("Downloading wallpaper...");

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (wallpaper_thread_.joinable()) {
        wallpaper_thread_.join();
    }

    wallpaper_thread_ = std::thread(
        &ApiClient::ExecuteWallpaperDownloadWorker,
        this,
        url,
        apply_lockscreen,
        apply_homescreen);
}

void ApiClient::DisconnectAccount() {
    TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
        acc = DriverAccountProfile{};
        acc.status = AccountSyncStatus::Disconnected;
        acc.status_message = "Account disconnected.";
    });
}

void ApiClient::Shutdown() {
    shutting_down_.store(true);
    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (job_thread_.joinable()) {
        job_thread_.join();
    }
    if (points_thread_.joinable()) {
        points_thread_.join();
    }
    if (wallpaper_thread_.joinable()) {
        wallpaper_thread_.join();
    }
}

void ApiClient::PostJobStartAsync(const JobStartRequest& req) {
    if (shutting_down_.load()) {
        return;
    }
    const auto cfg = ConfigManager::Instance().GetConfig();
    if (cfg.driver_token.empty()) {
        TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
            s.active_job_sync_status = "Not synced (No Token)";
        });
        return;
    }

    TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
        s.active_job_sync_status = "Registering job on server...";
    });

    nlohmann::json body = {
        {"gameId", req.game_id},
        {"gameMode", req.game_mode},
        {"marketType", req.market_type},
        {"sourceCity", req.source_city},
        {"destinationCity", req.destination_city},
        {"sourceCompany", req.source_company},
        {"destinationCompany", req.destination_company},
        {"cargoId", req.cargo_id},
        {"cargoName", req.cargo_name},
        {"cargoMassTons", req.cargo_mass_tons},
        {"plannedDistanceKm", req.planned_distance_km},
        {"vehicleBrand", req.vehicle_brand},
        {"vehicleModel", req.vehicle_model},
        {"vehicleInGameId", req.vehicle_in_game_id},
        {"fleetNumber", req.fleet_number},
        {"hasForbiddenMods", req.has_forbidden_mods},
        {"forbiddenModsSummary", req.forbidden_mods_summary},
        {"hardcoreSettings", {
            {"trafficOk", req.hc_traffic_ok},
            {"fatigueOk", req.hc_fatigue_ok},
            {"policeOk", req.hc_police_ok},
            {"parkingOk", req.hc_parking_ok},
            {"roadEventsOk", req.hc_road_events_ok},
            {"rainProbOk", req.hc_rain_prob_ok},
            {"advancedCouplingOk", req.hc_advanced_coupling_ok},
            {"fuelSimOk", req.hc_fuel_simulation_ok},
            {"airBrakeOk", req.hc_air_brake_ok}
        }}
    };

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (job_thread_.joinable()) {
        job_thread_.join();
    }
    job_thread_ = std::thread(
        &ApiClient::ExecuteJobPostWorker,
        this,
        "/api/tracker/jobs/start",
        body.dump(),
        "start");
}

void ApiClient::PostJobCompleteAsync(const JobCompleteRequest& req) {
    if (shutting_down_.load()) {
        return;
    }
    const auto cfg = ConfigManager::Instance().GetConfig();
    if (cfg.driver_token.empty()) {
        TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
            s.active_job_sync_status = "Not synced (No Token)";
        });
        return;
    }

    TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
        s.active_job_sync_status = "Submitting delivery paycheck...";
    });

    nlohmann::json fines_arr = nlohmann::json::array();
    for (const auto& f : req.fines) {
        fines_arr.push_back({
            {"offence", f.first},
            {"amount", f.second}
        });
    }

    nlohmann::json body = {
        {"jobId", req.job_id},
        {"gameId", req.game_id},
        {"gameMode", req.game_mode},
        {"distanceKm", req.distance_km},
        {"durationSeconds", req.duration_seconds},
        {"fuelUsedL", req.fuel_used_liters},
        {"maxSpeedKmh", req.max_speed_kmh},
        {"vehicleDamagePct", req.vehicle_damage_pct},
        {"trailerDamagePct", req.trailer_damage_pct},
        {"cargoDamagePct", req.cargo_damage_pct},
        {"inGameRevenue", req.in_game_revenue},
        {"isHardcore", req.is_hardcore},
        {"hardcoreRating", req.hardcore_rating},
        {"isHardcoreEligible", req.is_hardcore_eligible},
        {"isHardcoreEligibleAtStart", req.is_hardcore_eligible_at_start},
        {"hardcoreDisqualifiedDuringTrip", req.hardcore_disqualified_during_trip},
        {"hardcoreMidTripReason", req.hardcore_mid_trip_reason},
        {"speedingSeconds", req.speeding_seconds},
        {"restOverdueMinutes", req.rest_overdue_minutes},
        {"isLate", req.is_late},
        {"autoParkUsed", req.auto_park_used},
        {"warpDetected", req.warp_detected},
        {"hasForbiddenMods", req.has_forbidden_mods},
        {"forbiddenModsSummary", req.forbidden_mods_summary},
        {"hardcoreSettings", {
            {"trafficOk", req.hc_traffic_ok},
            {"fatigueOk", req.hc_fatigue_ok},
            {"policeOk", req.hc_police_ok},
            {"parkingOk", req.hc_parking_ok},
            {"roadEventsOk", req.hc_road_events_ok},
            {"rainProbOk", req.hc_rain_prob_ok},
            {"advancedCouplingOk", req.hc_advanced_coupling_ok},
            {"fuelSimOk", req.hc_fuel_simulation_ok},
            {"airBrakeOk", req.hc_air_brake_ok}
        }},
        {"vehicleInGameId", req.vehicle_in_game_id},
        {"fleetNumber", req.fleet_number},
        {"fines", fines_arr}
    };

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (job_thread_.joinable()) {
        job_thread_.join();
    }
    job_thread_ = std::thread(
        &ApiClient::ExecuteJobPostWorker,
        this,
        "/api/tracker/jobs/complete",
        body.dump(),
        "complete");
}

void ApiClient::PostJobCancelAsync(const JobCancelRequest& req) {
    if (shutting_down_.load()) {
        return;
    }
    const auto cfg = ConfigManager::Instance().GetConfig();
    if (cfg.driver_token.empty()) {
        return;
    }

    TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
        s.active_job_sync_status = "Recording job cancellation...";
    });

    nlohmann::json body = {
        {"jobId", req.job_id},
        {"gameId", req.game_id}
    };

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (job_thread_.joinable()) {
        job_thread_.join();
    }
    job_thread_ = std::thread(
        &ApiClient::ExecuteJobPostWorker,
        this,
        "/api/tracker/jobs/cancel",
        body.dump(),
        "cancel");
}

void ApiClient::PayPenaltyPointsAsync(int32_t points_to_pay) {
    if (shutting_down_.load() || points_to_pay <= 0) {
        return;
    }
    const auto cfg = ConfigManager::Instance().GetConfig();
    if (cfg.driver_token.empty()) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.penalty_action_message = "Token driver belum dikonfigurasi.";
            acc.penalty_action_is_error = true;
            acc.penalty_action_time_remaining = 5.0f;
        });
        return;
    }

    TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
        acc.penalty_action_message = "Memproses pembayaran poin penalti...";
        acc.penalty_action_is_error = false;
        acc.penalty_action_time_remaining = 3.0f;
    });

    nlohmann::json body = {
        {"pointsToPay", points_to_pay}
    };

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (points_thread_.joinable()) {
        points_thread_.join();
    }
    points_thread_ = std::thread(
        &ApiClient::ExecutePointsWorker,
        this,
        "/api/tracker/points/pay",
        body.dump(),
        "pay");
}

void ApiClient::UsePenaltyTicketAsync(int32_t tickets_to_use) {
    if (shutting_down_.load() || tickets_to_use <= 0) {
        return;
    }
    const auto cfg = ConfigManager::Instance().GetConfig();
    if (cfg.driver_token.empty()) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.penalty_action_message = "Token driver belum dikonfigurasi.";
            acc.penalty_action_is_error = true;
            acc.penalty_action_time_remaining = 5.0f;
        });
        return;
    }

    TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
        acc.penalty_action_message = "Menggunakan tiket hapus penalti...";
        acc.penalty_action_is_error = false;
        acc.penalty_action_time_remaining = 3.0f;
    });

    nlohmann::json body = {
        {"amountToUse", tickets_to_use}
    };

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (points_thread_.joinable()) {
        points_thread_.join();
    }
    points_thread_ = std::thread(
        &ApiClient::ExecutePointsWorker,
        this,
        "/api/tracker/points/use-ticket",
        body.dump(),
        "use_ticket");
}

void ApiClient::ExecutePointsWorker(
    std::string endpoint,
    std::string json_body,
    std::string action_type) {
    (void)action_type;
    try {
        const auto cfg = ConfigManager::Instance().GetConfig();
    if (cfg.driver_token.empty() || cfg.api_base_url.empty()) {
        return;
    }

    const std::string full_url = cfg.api_base_url + endpoint;
    const std::wstring wide_url = Utf8ToWide(full_url);

    URL_COMPONENTS url_comp = {};
    url_comp.dwStructSize = sizeof(url_comp);
    wchar_t host_name[256] = {0};
    wchar_t url_path[512] = {0};
    url_comp.lpszHostName = host_name;
    url_comp.dwHostNameLength = static_cast<DWORD>(std::size(host_name));
    url_comp.lpszUrlPath = url_path;
    url_comp.dwUrlPathLength = static_cast<DWORD>(std::size(url_path));

    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &url_comp)) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.penalty_action_message = "URL API tidak valid.";
            acc.penalty_action_is_error = true;
            acc.penalty_action_time_remaining = 5.0f;
        });
        return;
    }

    HINTERNET h_session = WinHttpOpen(
        L"NismaraTracker/1.0 (Windows x64)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!h_session) return;

    WinHttpSetTimeouts(h_session, 5000, 5000, 10000, 10000);

    HINTERNET h_connect = WinHttpConnect(
        h_session,
        host_name,
        url_comp.nPort,
        0);
    if (!h_connect) {
        WinHttpCloseHandle(h_session);
        return;
    }

    DWORD flags = (url_comp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET h_request = WinHttpOpenRequest(
        h_connect,
        L"POST",
        url_path,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);

    if (!h_request) {
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + Utf8ToWide(cfg.driver_token) + L"\r\n";
    headers += L"X-Plugin-Version: 1.0.2\r\n";

    BOOL sent = WinHttpSendRequest(
        h_request,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        const_cast<char*>(json_body.c_str()),
        static_cast<DWORD>(json_body.size()),
        static_cast<DWORD>(json_body.size()),
        0);

    if (!sent || !WinHttpReceiveResponse(h_request, nullptr)) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.penalty_action_message = "Koneksi ke tracker server gagal.";
            acc.penalty_action_is_error = true;
            acc.penalty_action_time_remaining = 5.0f;
        });
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(
        h_request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX);

    std::string response_body;
    DWORD bytes_available = 0;
    while (WinHttpQueryDataAvailable(h_request, &bytes_available) && bytes_available > 0) {
        std::vector<char> buf(bytes_available + 1, 0);
        DWORD bytes_read = 0;
        if (WinHttpReadData(h_request, buf.data(), bytes_available, &bytes_read) && bytes_read > 0) {
            response_body.append(buf.data(), bytes_read);
        }
    }

    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    try {
        nlohmann::json res_json = nlohmann::json::parse(response_body);
        const bool success = res_json.value("success", false);
        const std::string msg = success
                                    ? res_json.value("message", "Operasi berhasil!")
                                    : res_json.value("error", "Gagal memproses aksi penalti.");

        TelemetryState::Instance().WriteDriverAccount([&msg, success](DriverAccountProfile& acc) {
            acc.penalty_action_message = msg;
            acc.penalty_action_is_error = !success;
            acc.penalty_action_time_remaining = 6.0f;
        });

        if (success) {
            // Segera sinkronisasi ulang data profil, saldo NC, tiket safebox, dan poin penalti
            SyncAccountAsync();
        }
    } catch (...) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.penalty_action_message = "Respon server tidak valid.";
            acc.penalty_action_is_error = true;
            acc.penalty_action_time_remaining = 5.0f;
        });
    }
    } catch (...) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.penalty_action_message = "Gagal memproses permintaan (exception).";
            acc.penalty_action_is_error = true;
            acc.penalty_action_time_remaining = 5.0f;
        });
    }
}

void ApiClient::ExecuteJobPostWorker(
    std::string endpoint,
    std::string json_body,
    std::string event_kind) {
    try {
        const auto cfg = ConfigManager::Instance().GetConfig();
    if (cfg.driver_token.empty() || cfg.api_base_url.empty()) {
        return;
    }

    const std::string full_url = cfg.api_base_url + endpoint;
    const std::wstring wide_url = Utf8ToWide(full_url);

    URL_COMPONENTS url_comp = {};
    url_comp.dwStructSize = sizeof(url_comp);

    wchar_t host_name[256] = {0};
    wchar_t url_path[512] = {0};
    url_comp.lpszHostName = host_name;
    url_comp.dwHostNameLength = static_cast<DWORD>(std::size(host_name));
    url_comp.lpszUrlPath = url_path;
    url_comp.dwUrlPathLength = static_cast<DWORD>(std::size(url_path));

    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &url_comp)) {
        TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
            s.active_job_sync_status = "Invalid API URL";
        });
        return;
    }

    HINTERNET h_session = WinHttpOpen(
        L"NismaraTracker/1.0 (Windows x64; SCS Job Processor)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!h_session) {
        return;
    }

    WinHttpSetTimeouts(h_session, 5000, 5000, 10000, 10000);

    HINTERNET h_connect = WinHttpConnect(h_session, host_name, url_comp.nPort, 0);
    if (!h_connect) {
        WinHttpCloseHandle(h_session);
        TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
            s.active_job_sync_status = "Server Unreachable";
        });
        return;
    }

    const DWORD request_flags = (url_comp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET h_request = WinHttpOpenRequest(
        h_connect,
        L"POST",
        url_path,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        request_flags);

    if (!h_request) {
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + Utf8ToWide(cfg.driver_token) + L"\r\n";
    headers += L"X-Plugin-Version: 1.0.2\r\n";

    BOOL send_ok = WinHttpSendRequest(
        h_request,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        const_cast<char*>(json_body.data()),
        static_cast<DWORD>(json_body.size()),
        static_cast<DWORD>(json_body.size()),
        0);

    if (!send_ok || !WinHttpReceiveResponse(h_request, nullptr)) {
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        TelemetryState::Instance().Write([](TelemetrySnapshot& s) {
            s.active_job_sync_status = "Network error sending job data";
        });
        return;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(
        h_request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX);

    std::string response_body;
    DWORD bytes_avail = 0;
    while (WinHttpQueryDataAvailable(h_request, &bytes_avail) && bytes_avail > 0) {
        std::vector<char> chunk(bytes_avail);
        DWORD bytes_read = 0;
        if (WinHttpReadData(h_request, chunk.data(), bytes_avail, &bytes_read) && bytes_read > 0) {
            response_body.append(chunk.data(), bytes_read);
        } else {
            break;
        }
    }

    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    const auto parsed = nlohmann::json::parse(response_body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object() || !parsed.value("success", false)) {
        const std::string err_msg = (!parsed.is_discarded() && parsed.is_object())
                                        ? parsed.value("error", "Server rejected job event")
                                        : "Invalid server response";
        TelemetryState::Instance().Write([&err_msg](TelemetrySnapshot& s) {
            s.active_job_sync_status = "Error: " + err_msg;
        });
        return;
    }

    const auto data = parsed.contains("data") && parsed["data"].is_object() ? parsed["data"] : nlohmann::json::object();

    if (event_kind == "start") {
        const std::string job_id = data.value("jobId", "");
        const double locked_price = data.value("lockedCargoPrice", 1.5);
        const bool is_special = data.value("isSpecialContract", false);
        const bool resumed = data.value("resumed", false);
        const bool hc_start_ok = data.value("isHardcoreEligibleAtStart", true);
        const std::string hc_reason = data.value("hardcoreDisqualifyReason", "");

        TelemetryState::Instance().Write([&](TelemetrySnapshot& s) {
            s.active_job_db_id = job_id;
            s.active_job_locked_price = locked_price;
            s.active_job_is_special = is_special;
            s.active_job_sync_status = (resumed ? "Resumed Job #" : "Recording Job #") + job_id;
            s.hardcore_eligible_at_start = hc_start_ok;
            if (!hc_start_ok) {
                s.hardcore_disqualified = true;
                if (!hc_reason.empty()) {
                    s.hardcore_disqualify_reason = hc_reason;
                }
            }
        });
    } else if (event_kind == "complete") {
        const std::string job_id = data.value("jobId", "");
        const std::string game_mode = data.value("gameMode", "sp");
        const int32_t dist_km = data.value("distanceKm", 0);
        const int32_t rev_nc = data.value("revenue", 0);

        int32_t base_nc = 0;
        int32_t mp_bonus_nc = 0;
        int32_t hardcore_nc = 0;
        int32_t nplus_nc = 0;
        if (data.contains("nc") && data["nc"].is_object()) {
            const auto& nc_obj = data["nc"];
            base_nc = nc_obj.value("base", 0) + nc_obj.value("special", 0);
            mp_bonus_nc = nc_obj.value("truckersmp", 0) + nc_obj.value("haulmp", 0);
            hardcore_nc = nc_obj.value("hardcore", 0);
            nplus_nc = nc_obj.value("nismaraplus", 0) + nc_obj.value("booster", 0) + nc_obj.value("event", 0) + nc_obj.value("userBoost", 0);
        }

        int32_t cost_nc = 0;
        if (data.contains("ncCost") && data["ncCost"].is_object()) {
            cost_nc = data["ncCost"].value("total", 0);
        }

        int32_t tax_nc = 0;
        if (data.contains("tax") && data["tax"].is_object()) {
            tax_nc = data["tax"].value("amount", 0);
        }

        int32_t total_xp = 0;
        if (data.contains("xp") && data["xp"].is_object()) {
            total_xp = data["xp"].value("total", 0);
        }

        int32_t pen_pts = 0;
        if (data.contains("penalty") && data["penalty"].is_object()) {
            pen_pts = data["penalty"].value("total", 0);
        }

        const bool is_hc = data.value("hardcoreEligible", false);
        const double hc_pts = data.value("hardcorePoints", 0.0);
        int32_t stars_dist = 0, stars_mass = 0, stars_dmg = 0, stars_fuel = 0, stars_fines = 0, stars_total = 0;
        if (data.contains("deliveryRating") && data["deliveryRating"].is_object()) {
            const auto& r = data["deliveryRating"];
            stars_dist = r.value("distanceStars", 0);
            stars_mass = r.value("massStars", 0);
            stars_dmg = r.value("damageStars", 0);
            stars_fuel = r.value("fuelStars", 0);
            stars_fines = r.value("finesStars", 0);
            stars_total = r.value("totalStars", 0);
        }
        const std::string hc_reason = data.value("hardcoreDisqualifyReason", "");

        TelemetryState::Instance().Write([&](TelemetrySnapshot& s) {
            s.active_job_db_id.clear();
            s.active_job_sync_status = "Completed Job #" + job_id + " (+" + std::to_string(rev_nc) + " NC)";
            s.has_last_job_receipt = true;
            s.last_receipt_job_id = job_id;
            s.last_receipt_game_mode = game_mode;
            s.last_receipt_distance_km = dist_km;
            s.last_receipt_revenue_nc = rev_nc;
            s.last_receipt_base_nc = base_nc;
            s.last_receipt_mp_bonus_nc = mp_bonus_nc;
            s.last_receipt_hardcore_nc = hardcore_nc;
            s.last_receipt_nplus_nc = nplus_nc;
            s.last_receipt_cost_nc = cost_nc;
            s.last_receipt_tax_nc = tax_nc;
            s.last_receipt_xp = total_xp;
            s.last_receipt_penalty_pts = pen_pts;
            s.last_receipt_is_hardcore = is_hc;
            s.last_receipt_hardcore_points = hc_pts;
            s.last_receipt_stars_distance = stars_dist;
            s.last_receipt_stars_mass = stars_mass;
            s.last_receipt_stars_damage = stars_dmg;
            s.last_receipt_stars_fuel = stars_fuel;
            s.last_receipt_stars_fines = stars_fines;
            s.last_receipt_stars_total = stars_total;
            s.last_receipt_disqualify_reason = hc_reason;
        });

        // Refresh driver wallet NC, XP, garage fuel, and fleet wear
        SyncAccountAsync();
    } else if (event_kind == "cancel") {
        const std::string job_id = data.value("jobId", "");
        const int32_t pen = data.value("penaltyPointsAdded", 5);
        TelemetryState::Instance().Write([&](TelemetrySnapshot& s) {
            s.active_job_db_id.clear();
            s.active_job_sync_status = "Job #" + job_id + " Canceled (+" + std::to_string(pen) + " Penalty Pts)";
        });

        // Refresh driver penalty points
        SyncAccountAsync();
    }
    } catch (...) {}
}

void ApiClient::ExecuteSyncWorker(
    std::string base_url,
    std::string token,
    std::string steam_id,
    std::string game_id) {
    struct ScopeGuard {
        std::atomic<bool>& flag;
        ~ScopeGuard() { flag.store(false); }
    } guard{is_syncing_};

    try {
        const std::string full_url = base_url + "/api/tracker/me";
        LogDebugToFile("ExecuteSyncWorker connecting to: " + full_url);
        const std::wstring wide_url = Utf8ToWide(full_url);

    URL_COMPONENTS url_comp = {};
    url_comp.dwStructSize = sizeof(url_comp);

    wchar_t host_name[256] = {0};
    wchar_t url_path[512] = {0};
    url_comp.lpszHostName = host_name;
    url_comp.dwHostNameLength = static_cast<DWORD>(std::size(host_name));
    url_comp.lpszUrlPath = url_path;
    url_comp.dwUrlPathLength = static_cast<DWORD>(std::size(url_path));

    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &url_comp)) {
        TelemetryState::Instance().WriteDriverAccount([&full_url](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message = "Invalid API URL format: " + full_url;
        });
        return;
    }

    HINTERNET h_session = WinHttpOpen(
        L"NismaraTracker/1.0 (Windows x64; SCS Telemetry Plugin)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!h_session) {
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message = "Failed to initialize WinHTTP session.";
        });
        return;
    }

    WinHttpSetTimeouts(h_session, 5000, 5000, 8000, 8000);

    HINTERNET h_connect = WinHttpConnect(h_session, host_name, url_comp.nPort, 0);
    if (!h_connect) {
        WinHttpCloseHandle(h_session);
        TelemetryState::Instance().WriteDriverAccount([&base_url](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message = "Failed to connect to host: " + base_url;
        });
        return;
    }

    const DWORD request_flags = (url_comp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET h_request = WinHttpOpenRequest(
        h_connect,
        L"POST",
        url_path,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        request_flags);

    if (!h_request) {
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message = "Failed to create HTTP request.";
        });
        return;
    }

    nlohmann::json req_json = {
        {"token", token},
        {"gameId", game_id},
        {"pluginVersion", "1.0.2"},
        {"steamId", steam_id}
    };
    const std::string body_str = req_json.dump();

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + Utf8ToWide(token) + L"\r\n";
    headers += L"X-Plugin-Version: 1.0.2\r\n";
    if (!game_id.empty()) {
        headers += L"X-Game-Id: " + Utf8ToWide(game_id) + L"\r\n";
    }
    if (!steam_id.empty()) {
        headers += L"X-Steam-Id: " + Utf8ToWide(steam_id) + L"\r\n";
    }

    BOOL send_ok = WinHttpSendRequest(
        h_request,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        const_cast<char*>(body_str.data()),
        static_cast<DWORD>(body_str.size()),
        static_cast<DWORD>(body_str.size()),
        0);

    if (!send_ok || !WinHttpReceiveResponse(h_request, nullptr)) {
        const DWORD err = GetLastError();
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);

        TelemetryState::Instance().WriteDriverAccount([&base_url, err](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message =
                "Could not reach " + base_url + " (WinHTTP Error " + std::to_string(err) + ").";
        });
        return;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(
        h_request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX);

    std::string response_body;
    DWORD bytes_avail = 0;
    while (WinHttpQueryDataAvailable(h_request, &bytes_avail) && bytes_avail > 0) {
        std::vector<char> chunk(bytes_avail);
        DWORD bytes_read = 0;
        if (WinHttpReadData(h_request, chunk.data(), bytes_avail, &bytes_read) && bytes_read > 0) {
            response_body.append(chunk.data(), bytes_read);
        } else {
            break;
        }
    }

    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    LogDebugToFile("ExecuteSyncWorker received response: HTTP " + std::to_string(status_code) + ", bytes=" + std::to_string(response_body.size()));

    const auto parsed = nlohmann::json::parse(response_body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        LogDebugToFile("ExecuteSyncWorker failed to parse JSON response!");
        TelemetryState::Instance().WriteDriverAccount([status_code](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message =
                "Invalid JSON response from server (HTTP " + std::to_string(status_code) + ").";
        });
        return;
    }

    const bool success = parsed.value("success", false);
    if (!success || status_code != 200 || !parsed.contains("driver") || !parsed["driver"].is_object()) {
        const std::string err_code = parsed.value("errorCode", "");
        const std::string min_ver = parsed.value("minVersion", "1.0.0");
        const std::string server_msg =
            parsed.value("error", parsed.value("message", "Authentication failed (HTTP " + std::to_string(status_code) + ")."));

        TelemetryState::Instance().WriteDriverAccount([status_code, &server_msg, &err_code, &min_ver](DriverAccountProfile& acc) {
            if (status_code == 426 || err_code == "OUTDATED_VERSION") {
                acc.status = AccountSyncStatus::VersionOutdated;
                acc.is_version_outdated = true;
                acc.required_min_version = min_ver;
                acc.status_message = "UPDATE DIPERLUKAN (v" + min_ver + "): Buka Nismara Companion untuk update.";
            } else if (status_code == 401 || status_code == 403) {
                acc.status = AccountSyncStatus::InvalidToken;
                acc.is_version_outdated = false;
                acc.status_message = server_msg;
            } else {
                acc.status = AccountSyncStatus::NetworkError;
                acc.is_version_outdated = false;
                acc.status_message = server_msg;
            }
        });
        return;
    }

    const auto& drv = parsed["driver"];

    TelemetryState::Instance().WriteDriverAccount([&drv](DriverAccountProfile& acc) {
        acc.status = AccountSyncStatus::Authenticated;
        acc.is_version_outdated = false;
        acc.last_sync_timestamp = GetLocalTimeString();
        acc.status_message = "Synced at " + acc.last_sync_timestamp;

        acc.user_id = drv.value("id", "");
        acc.name = drv.value("name", "Driver");
        acc.email = drv.value("email", "");
        acc.image_url = drv.value("image", "");
        acc.background_url = drv.value("backgroundUrl", "");
        acc.banner_url = drv.value("bannerUrl", "");
        acc.discord_id = drv.value("discordId", "");
        acc.trucky_id = drv.value("truckyId", int64_t{0});
        acc.steam_id = drv.value("steamId", "");
        acc.truckersmp_id = drv.value("truckersmpId", "");

        acc.discord_role = drv.value("discordRole", "driver");
        acc.trucky_rank = drv.value("truckyRank", "Driver");
        acc.trucky_rank_color = drv.value("truckyRankColor", "#ffffff");
        acc.trucky_role = drv.value("truckyRole", "Driver");
        acc.trucky_role_color = drv.value("truckyRoleColor", "#ffffff");

        acc.level = drv.value("level", 1);
        acc.xp = drv.value("xp", int64_t{0});
        acc.nc = drv.value("nc", 0.0);
        acc.total_nc = drv.value("totalNc", acc.nc);
        acc.points = drv.value("points", 0);
        acc.penalty_points = drv.value("penaltyPoints", acc.points);
        acc.point_price = drv.value("pointPrice", 3000);
        acc.discount_booster = drv.value("discountBooster", 0);
        acc.discount_nismara_plus = drv.value("discountNismaraPlus", 0);
        acc.total_discount = drv.value("totalDiscount", acc.discount_booster + acc.discount_nismara_plus);
        acc.final_point_price = drv.value("finalPointPrice", std::max(0, acc.point_price - acc.total_discount));
        acc.safebox_tickets = drv.value("safeboxTickets", 0);
        acc.max_penalty_tickets = drv.value("maxPenaltyTickets", 10);
        acc.joined_convoy = drv.value("joinedConvoy", 0);

        acc.is_driver = drv.value("isDriver", true);
        acc.is_on_leave = drv.value("isOnLeave", false);
        acc.is_booster = drv.value("isBooster", false);
        acc.is_guild_member = drv.value("isGuildMember", false);
        acc.is_tmp_driver = drv.value("isTmpDriver", false);
        acc.is_haulmp_driver = drv.value("isHaulmpDriver", false);
        acc.is_best_employee_ets2 = drv.value("isBestEmployeeETS2", false);
        acc.is_best_employee_ats = drv.value("isBestEmployeeATS", false);

        if (drv.contains("insurance") && drv["insurance"].is_object()) {
            const auto& ins = drv["insurance"];
            acc.insurance_active = ins.value("status", false);
            acc.insurance_rating = ins.value("rating", 0);
            acc.insurance_started_at = ins.value("startedAt", "");
            acc.insurance_expired_at = ins.value("expiredAt", "");
        }

        if (drv.contains("nismaraplus") && drv["nismaraplus"].is_object()) {
            const auto& np = drv["nismaraplus"];
            acc.nismara_plus_active = np.value("status", false);
            acc.nismara_plus_started_at = np.value("startedAt", "");
            acc.nismara_plus_expired_at = np.value("expiredAt", "");
        }

        if (drv.contains("ncBoost") && drv["ncBoost"].is_object()) {
            const auto& boost = drv["ncBoost"];
            acc.nc_boost_active = boost.value("active", false);
            acc.nc_boost_multiplier = boost.value("multiplier", 0.0f);
            acc.nc_boost_voucher_title = boost.value("voucherTitle", "");
            acc.nc_boost_voucher_code = boost.value("voucherCode", "");
            acc.nc_boost_expired_at = boost.value("expiredAt", "");
        }

        if (drv.contains("topManager") && drv["topManager"].is_object()) {
            const auto& tm = drv["topManager"];
            acc.top_manager_active = tm.value("status", false);
            acc.top_manager_month = tm.value("month", "");
            acc.top_manager_expired_at = tm.value("expiredAt", "");
        }

        acc.garage = DriverGarageSummary{};
        if (drv.contains("garage") && drv["garage"].is_object()) {
            const auto& g = drv["garage"];
            acc.garage.has_garage = true;
            acc.garage.status = g.value("status", "operational");
            acc.garage.fleet_slot = g.value("fleetSlot", 1);
            acc.garage.fleet_slot_used = g.value("fleetSlotUsed", 0);
            acc.garage.fleet_slot_level = g.value("fleetSlotLevel", acc.garage.fleet_slot);
            acc.garage.fuel_stock = g.value("fuelStock", 0.0);
            acc.garage.fuel_capacity = g.value("fuelCapacity", 2000.0);
            const int32_t derived_fuel_level = std::max(
                1, static_cast<int32_t>(std::round((acc.garage.fuel_capacity - 1000.0) / 1000.0)));
            acc.garage.fuel_tank_level = g.value("fuelTankLevel", derived_fuel_level);
            acc.garage.safebox_stock = g.value("safeboxStock", 0);
            acc.garage.safebox_level = g.value("safeboxLevel", 1);
            acc.garage.max_penalty_tickets = g.value("maxPenaltyTickets", 10 + (acc.garage.safebox_level - 1) * 5);

            // Calculate exact Nismara operational cost fallbacks when server response omits them
            double fallback_fleet_op = 0.0;
            for (int32_t i = 2; i <= acc.garage.fleet_slot_level; ++i) {
                const int32_t tier = (i - 1) / 3;
                fallback_fleet_op += 250.0 + (tier * 250.0);
            }
            double fallback_fuel_op = 0.0;
            for (int32_t i = 2; i <= acc.garage.fuel_tank_level; ++i) {
                const int32_t tier = (i - 1) / 5;
                fallback_fuel_op += 200.0 + (tier * 100.0);
            }
            double fallback_safebox_op = 0.0;
            for (int32_t i = 2; i <= acc.garage.safebox_level; ++i) {
                const int32_t tier = (i - 2) / 3;
                fallback_safebox_op += 250.0 + (tier * 150.0);
            }

            if (g.contains("fleet_operational_cost") && g["fleet_operational_cost"].is_number()) {
                acc.garage.fleet_operational_cost = g["fleet_operational_cost"].get<double>();
            } else if (g.contains("fleetOperationalCost") && g["fleetOperationalCost"].is_number()) {
                acc.garage.fleet_operational_cost = g["fleetOperationalCost"].get<double>();
            } else {
                acc.garage.fleet_operational_cost = fallback_fleet_op;
            }

            if (g.contains("fuel_operational_cost") && g["fuel_operational_cost"].is_number()) {
                acc.garage.fuel_operational_cost = g["fuel_operational_cost"].get<double>();
            } else if (g.contains("fuelOperationalCost") && g["fuelOperationalCost"].is_number()) {
                acc.garage.fuel_operational_cost = g["fuelOperationalCost"].get<double>();
            } else {
                acc.garage.fuel_operational_cost = fallback_fuel_op;
            }

            if (g.contains("safebox_operational_cost") && g["safebox_operational_cost"].is_number()) {
                acc.garage.safebox_operational_cost = g["safebox_operational_cost"].get<double>();
            } else if (g.contains("safeboxOperationalCost") && g["safeboxOperationalCost"].is_number()) {
                acc.garage.safebox_operational_cost = g["safeboxOperationalCost"].get<double>();
            } else {
                acc.garage.safebox_operational_cost = fallback_safebox_op;
            }

            const double fallback_total_op =
                acc.garage.fleet_operational_cost + acc.garage.fuel_operational_cost + acc.garage.safebox_operational_cost;
            if (g.contains("operational_cost") && g["operational_cost"].is_number()) {
                acc.garage.operational_cost = g["operational_cost"].get<double>();
            } else if (g.contains("operationalCost") && g["operationalCost"].is_number()) {
                acc.garage.operational_cost = g["operationalCost"].get<double>();
            } else {
                acc.garage.operational_cost = fallback_total_op;
            }

            if (g.contains("next_payment_date") && g["next_payment_date"].is_string()) {
                acc.garage.next_payment_date = g["next_payment_date"].get<std::string>();
            } else if (g.contains("nextPaymentDate") && g["nextPaymentDate"].is_string()) {
                acc.garage.next_payment_date = g["nextPaymentDate"].get<std::string>();
            }
        }

        acc.fleets.clear();
        if (drv.contains("fleets") && drv["fleets"].is_array()) {
            for (const auto& item : drv["fleets"]) {
                if (!item.is_object()) {
                    continue;
                }
                DriverFleetVehicle fv;
                fv.id = item.value("id", "");
                fv.mongo_id = item.value("mongoId", "");
                fv.fleet_number = item.value("fleetNumber", "");
                fv.fleet_name = item.value("fleetName", "");
                fv.custom_name = item.value("customName", "");
                fv.game_id = item.value("gameId", "1");
                fv.odometer = item.value("odometer", 0.0);
                fv.wheels = item.value("wheels", "4x2");
                fv.status = item.value("status", "active");
                fv.has_insurance = item.value("hasInsurance", false);
                fv.maintenance_needed =
                    item.value("maintenanceNeeded", item.value("maintenance_needed", false));
                fv.is_assigned_driver = item.value("isAssignedDriver", true);
                fv.in_game_id = item.value("inGameId", "");
                fv.model_name = item.value("modelName", "");
                fv.model_type = item.value("modelType", "truck");
                fv.photo_url = item.value("photoUrl", "");
                fv.brand_name = item.value("brandName", "");

                if (item.contains("maintenanceStartDate") && item["maintenanceStartDate"].is_string()) {
                    fv.maintenance_start_date = item["maintenanceStartDate"].get<std::string>();
                } else if (item.contains("maintenance_start_date") && item["maintenance_start_date"].is_string()) {
                    fv.maintenance_start_date = item["maintenance_start_date"].get<std::string>();
                }

                if (item.contains("maintenanceEndDate") && item["maintenanceEndDate"].is_string()) {
                    fv.maintenance_end_date = item["maintenanceEndDate"].get<std::string>();
                } else if (item.contains("maintenance_end_date") && item["maintenance_end_date"].is_string()) {
                    fv.maintenance_end_date = item["maintenance_end_date"].get<std::string>();
                }

                fv.jobs_count = item.value("jobsCount", item.value("jobs_count", 0));
                fv.total_revenue = item.value("totalRevenue", item.value("total_revenue", 0.0));
                fv.total_fuel_used = item.value("totalFuelUsed", item.value("total_fuel_used", 0.0));
                fv.total_damage_cost = item.value("totalDamageCost", item.value("total_damage_cost", 0.0));

                if (item.contains("wear") && item["wear"].is_object()) {
                    const auto& w = item["wear"];
                    fv.unfix_engine = w.value("unfixEngine", 0.0f);
                    fv.unfix_transmission = w.value("unfixTransmission", 0.0f);
                    fv.unfix_brakes = w.value("unfixBrakes", 0.0f);
                    fv.unfix_tires = w.value("unfixTires", 0.0f);
                }

                if (item.contains("maintenance") && item["maintenance"].is_object()) {
                    const auto& m = item["maintenance"];
                    fv.maint_engine = m.value("engine", 50000.0);
                    fv.maint_tires = m.value("tires", 20000.0);
                    fv.maint_transmission = m.value("transmission", 70000.0);
                    fv.maint_brakes = m.value("brakes", 35000.0);
                }

                if (item.contains("lastMaintenance") && item["lastMaintenance"].is_object()) {
                    const auto& lm = item["lastMaintenance"];
                    fv.last_maint_engine = lm.value("engine", 0.0);
                    fv.last_maint_tires = lm.value("tires", 0.0);
                    fv.last_maint_transmission = lm.value("transmission", 0.0);
                    fv.last_maint_brakes = lm.value("brakes", 0.0);
                } else if (item.contains("last_maintenance") && item["last_maintenance"].is_object()) {
                    const auto& lm = item["last_maintenance"];
                    fv.last_maint_engine = lm.value("engine", 0.0);
                    fv.last_maint_tires = lm.value("tires", 0.0);
                    fv.last_maint_transmission = lm.value("transmission", 0.0);
                    fv.last_maint_brakes = lm.value("brakes", 0.0);
                }

                acc.fleets.push_back(std::move(fv));
            }
        }
    });

    if (drv.contains("ongoingJob") && drv["ongoingJob"].is_object()) {
        const auto& oj = drv["ongoingJob"];
        const std::string oj_id = oj.value("jobId", "");
        const double oj_price = oj.value("lockedCargoPrice", 1.5);
        const bool oj_special = oj.value("isSpecialContract", false);
        if (!oj_id.empty()) {
            TelemetryState::Instance().Write([&](TelemetrySnapshot& s) {
                if (s.active_job_db_id.empty()) {
                    s.active_job_db_id = oj_id;
                    s.active_job_locked_price = oj_price;
                    s.active_job_is_special = oj_special;
                    s.active_job_sync_status = "Synced Ongoing Job #" + oj_id;
                }
            });
        }
    }

    // Immediately match the driver's synced fleets against the currently driven in-game vehicle
    TelemetryState::Instance().MatchActiveFleet();

    const auto acc_check = TelemetryState::Instance().GetDriverAccount();
    LogDebugToFile("ExecuteSyncWorker finished: driver='" + acc_check.name +
                   "', fleets=" + std::to_string(acc_check.fleets.size()) +
                   ", garage.has=" + (acc_check.garage.has_garage ? "1" : "0") +
                   ", garage.slots=" + std::to_string(acc_check.garage.fleet_slot) +
                   ", garage.fuel=" + std::to_string(acc_check.garage.fuel_stock));
    } catch (const std::exception& ex) {
        LogDebugToFile(std::string("ExecuteSyncWorker std::exception: ") + ex.what());
        TelemetryState::Instance().WriteDriverAccount([&ex](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message = std::string("Sync Error: ") + ex.what();
        });
    } catch (...) {
        LogDebugToFile("ExecuteSyncWorker unknown exception caught");
        TelemetryState::Instance().WriteDriverAccount([](DriverAccountProfile& acc) {
            acc.status = AccountSyncStatus::NetworkError;
            acc.status_message = "Unexpected error while synchronizing profile.";
        });
    }
}

void ApiClient::ExecuteWallpaperDownloadWorker(
    std::string url,
    bool apply_lockscreen,
    bool apply_homescreen) {
    struct ScopeGuard {
        std::atomic<bool>& flag;
        ~ScopeGuard() { flag.store(false); }
    } guard{is_downloading_wallpaper_};

    try {
        const std::wstring wide_url = Utf8ToWide(url);

    URL_COMPONENTS url_comp = {};
    url_comp.dwStructSize = sizeof(url_comp);

    wchar_t host_name[256] = {0};
    wchar_t url_path[1024] = {0};
    url_comp.lpszHostName = host_name;
    url_comp.dwHostNameLength = static_cast<DWORD>(std::size(host_name));
    url_comp.lpszUrlPath = url_path;
    url_comp.dwUrlPathLength = static_cast<DWORD>(std::size(url_path));

    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &url_comp)) {
        SetWallpaperStatus("Invalid wallpaper URL.");
        return;
    }

    HINTERNET h_session = WinHttpOpen(
        L"NismaraTracker/1.0 (Windows x64; Wallpaper Fetcher)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!h_session) {
        SetWallpaperStatus("WinHTTP session failed.");
        return;
    }

    WinHttpSetTimeouts(h_session, 5000, 5000, 12000, 12000);

    HINTERNET h_connect = WinHttpConnect(h_session, host_name, url_comp.nPort, 0);
    if (!h_connect) {
        WinHttpCloseHandle(h_session);
        SetWallpaperStatus("Could not connect to image host.");
        return;
    }

    const DWORD request_flags = (url_comp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET h_request = WinHttpOpenRequest(
        h_connect,
        L"GET",
        url_path,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        request_flags);

    if (!h_request) {
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        SetWallpaperStatus("Failed to create image GET request.");
        return;
    }

    if (!WinHttpSendRequest(h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(h_request, nullptr)) {
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        SetWallpaperStatus("Failed to download image from URL.");
        return;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(
        h_request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX);

    if (status_code != 200) {
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        SetWallpaperStatus("Server returned HTTP " + std::to_string(status_code) + " for image.");
        return;
    }

    std::vector<uint8_t> image_bytes;
    DWORD bytes_avail = 0;
    while (WinHttpQueryDataAvailable(h_request, &bytes_avail) && bytes_avail > 0) {
        std::vector<uint8_t> chunk(bytes_avail);
        DWORD bytes_read = 0;
        if (WinHttpReadData(h_request, chunk.data(), bytes_avail, &bytes_read) && bytes_read > 0) {
            image_bytes.insert(image_bytes.end(), chunk.begin(), chunk.begin() + bytes_read);
            if (image_bytes.size() > 25 * 1024 * 1024) { // 25 MB safety cap
                break;
            }
        } else {
            break;
        }
    }

    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    if (image_bytes.size() < 64) {
        SetWallpaperStatus("Downloaded image is empty or invalid.");
        return;
    }

    const std::string ext = GuessExtensionFromUrl(url);
    const std::string filename = "nismara_web_wallpaper" + ext;
    const auto dir = ConfigManager::Instance().GetWallpapersDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto out_path = dir / filename;

    std::ofstream out_file(out_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out_file.is_open()) {
        SetWallpaperStatus("Failed to save wallpaper into plugins/wallpapers/.");
        return;
    }

    out_file.write(reinterpret_cast<const char*>(image_bytes.data()), static_cast<std::streamsize>(image_bytes.size()));
    out_file.flush();
    out_file.close();

    if (apply_lockscreen) {
        ConfigManager::Instance().SetLockscreenWallpaper(filename);
    }
    if (apply_homescreen) {
        ConfigManager::Instance().SetHomescreenWallpaper(filename);
    }
    ConfigManager::Instance().Save();
    nismara::ui::TextureManager::Instance().RequestReload();

    SetWallpaperStatus("Wallpaper downloaded & applied (" + filename + ")!");
    } catch (...) {
        SetWallpaperStatus("Failed to download wallpaper.");
    }
}

} // namespace nismara::core
