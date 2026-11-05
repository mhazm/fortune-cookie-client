#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace nismara::core {

struct JobStartRequest {
    std::string game_id{"1"};          // "1" = ETS2, "2" = ATS
    std::string game_mode{"sp"};       // "sp", "truckersmp", "haulmp"
    std::string market_type{"Freight Market"};
    std::string source_city{};
    std::string destination_city{};
    std::string source_company{};
    std::string destination_company{};
    std::string cargo_id{};
    std::string cargo_name{};
    float cargo_mass_tons{0.0f};
    uint32_t planned_distance_km{0};
    std::string vehicle_brand{};
    std::string vehicle_model{};
    std::string vehicle_in_game_id{};
    std::string fleet_number{};
};

struct JobCompleteRequest {
    std::string job_id{};
    std::string game_id{"1"};
    std::string game_mode{"sp"};
    uint32_t distance_km{0};
    int64_t duration_seconds{0};
    float fuel_used_liters{0.0f};
    float max_speed_kmh{0.0f};
    int32_t vehicle_damage_pct{0};
    int32_t trailer_damage_pct{0};
    int32_t cargo_damage_pct{0};
    uint64_t in_game_revenue{0};
    bool is_hardcore{false};
    float hardcore_rating{5.0f};
    bool is_hardcore_eligible{false};
    float speeding_seconds{0.0f};
    int32_t rest_overdue_minutes{0};
    bool is_late{false};
    bool auto_park_used{false};
    bool warp_detected{false};
    bool has_forbidden_mods{false};
    std::string forbidden_mods_summary{};
    bool hc_traffic_ok{false};
    bool hc_fatigue_ok{false};
    bool hc_police_ok{false};
    bool hc_parking_ok{false};
    bool hc_road_events_ok{false};
    bool hc_rain_prob_ok{false};
    bool hc_advanced_coupling_ok{false};
    bool hc_fuel_simulation_ok{false};
    bool hc_air_brake_ok{false};
    std::string vehicle_in_game_id{};
    std::string fleet_number{};
    std::vector< std::pair<std::string, int64_t> > fines{};
};

struct JobCancelRequest {
    std::string job_id{};
    std::string game_id{"1"};
};

/**
 * @brief Asynchronous native WinHTTP client for synchronizing the driver's
 *        MongoDB account profile from nismara-tracker-server (/api/tracker/me),
 *        submitting job lifecycle events (/api/tracker/jobs/*),
 *        and downloading remote wallpaper images.
 */
class ApiClient {
public:
    static ApiClient& Instance();

    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    /**
     * @brief Triggers an asynchronous account synchronization on a background worker thread.
     */
    void SyncAccountAsync();

    /**
     * @brief Submits a job start event (POST /api/tracker/jobs/start) on a background worker thread.
     */
    void PostJobStartAsync(const JobStartRequest& req);

    /**
     * @brief Submits a job completion event (POST /api/tracker/jobs/complete) on a background worker thread.
     */
    void PostJobCompleteAsync(const JobCompleteRequest& req);

    /**
     * @brief Submits a job cancellation event (POST /api/tracker/jobs/cancel) on a background worker thread.
     */
    void PostJobCancelAsync(const JobCancelRequest& req);

    /**
     * @brief Redeems/pays penalty points using Nismara Cash (NC) via POST /api/tracker/points/pay.
     */
    void PayPenaltyPointsAsync(int32_t points_to_pay);

    /**
     * @brief Uses penalty removal tickets from garage Safebox via POST /api/tracker/points/use-ticket.
     */
    void UsePenaltyTicketAsync(int32_t tickets_to_use);

    /**
     * @brief Downloads a wallpaper image from an HTTP/HTTPS URL on a background thread,
     *        saves it into plugins/wallpapers/, and applies it to Lock Screen / Home Screen.
     */
    void DownloadWallpaperFromUrlAsync(
        const std::string& url,
        bool apply_lockscreen,
        bool apply_homescreen);

    /**
     * @brief Clears current authenticated profile state and marks status as Disconnected.
     */
    void DisconnectAccount();

    /**
     * @brief Returns true if an HTTP sync request is currently in progress.
     */
    bool IsSyncing() const noexcept { return is_syncing_.load(); }

    /**
     * @brief Returns true if a wallpaper download is currently in progress.
     */
    bool IsDownloadingWallpaper() const noexcept { return is_downloading_wallpaper_.load(); }

    /**
     * @brief Returns status text for the last wallpaper download operation.
     */
    std::string GetWallpaperDownloadStatus() const;

    /**
     * @brief Waits for any active background request thread to complete before DLL unload.
     */
    void Shutdown();

private:
    ApiClient() = default;
    ~ApiClient() = default;

    void ExecuteSyncWorker(std::string base_url, std::string token, std::string steam_id, std::string game_id);
    void ExecuteJobPostWorker(std::string endpoint, std::string json_body, std::string event_kind);
    void ExecutePointsWorker(std::string endpoint, std::string json_body, std::string action_type);
    void ExecuteWallpaperDownloadWorker(std::string url, bool apply_lockscreen, bool apply_homescreen);
    void SetWallpaperStatus(const std::string& msg);

    std::mutex thread_mutex_;
    std::thread worker_thread_{};
    std::thread job_thread_{};
    std::thread points_thread_{};
    std::thread wallpaper_thread_{};
    mutable std::mutex status_mutex_;
    std::string wallpaper_status_{};
    std::atomic<bool> is_syncing_{false};
    std::atomic<bool> is_downloading_wallpaper_{false};
    std::atomic<bool> shutting_down_{false};
};

} // namespace nismara::core
