#pragma once

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

namespace nismara::core {

enum class AccountSyncStatus {
    Disconnected,
    Connecting,
    Authenticated,
    InvalidToken,
    NetworkError,
    VersionOutdated
};

/**
 * @brief Represents a single vehicle from the MongoDB `fleets` collection
 *        joined with `fleetstores` and `fleetbrands`.
 */
struct DriverFleetVehicle {
    std::string id{};
    std::string mongo_id{};
    std::string fleet_number{};  // e.g. "NL-074"
    std::string fleet_name{};    // e.g. "NL-074 - Renault Magnum"
    std::string custom_name{};   // e.g. "Si Manies"
    std::string game_id{"1"};    // "1" = ETS2, "2" = ATS
    double odometer{0.0};
    std::string wheels{"4x2"};
    std::string status{"active"};
    bool has_insurance{false};
    bool maintenance_needed{false};
    bool is_assigned_driver{true};

    // Joined from fleetstores & fleetbrands
    std::string in_game_id{};    // e.g. "vehicle.renault.magnum"
    std::string model_name{};    // e.g. "Magnum"
    std::string model_type{"truck"};
    std::string photo_url{};
    std::string brand_name{};    // e.g. "Renault"

    // Permanent / Unfixed Wear % in MongoDB
    float unfix_engine{0.0f};
    float unfix_transmission{0.0f};
    float unfix_brakes{0.0f};
    float unfix_tires{0.0f};

    // Next Maintenance Odometer Targets (km)
    double maint_engine{50000.0};
    double maint_tires{20000.0};
    double maint_transmission{70000.0};
    double maint_brakes{35000.0};

    // Last Maintenance Odometer (km)
    double last_maint_engine{0.0};
    double last_maint_tires{0.0};
    double last_maint_transmission{0.0};
    double last_maint_brakes{0.0};

    // Maintenance Schedule & Fleet Stats
    std::string maintenance_start_date{};
    std::string maintenance_end_date{};
    int32_t jobs_count{0};
    double total_revenue{0.0};
    double total_fuel_used{0.0};
    double total_damage_cost{0.0};
};

/**
 * @brief Represents the driver's personal garage from the MongoDB `garages` collection.
 */
struct DriverGarageSummary {
    bool has_garage{false};
    std::string status{"operational"};
    int32_t fleet_slot{1};
    int32_t fleet_slot_used{0};
    int32_t fleet_slot_level{1};
    double fuel_stock{0.0};
    double fuel_capacity{2000.0};
    int32_t fuel_tank_level{1};
    int32_t safebox_stock{0}; // Tiket Hapus Penalti
    int32_t safebox_level{1};
    int32_t max_penalty_tickets{10}; // 10 + (safebox_level - 1) * 5
    double operational_cost{0.0};
    double fleet_operational_cost{0.0};
    double fuel_operational_cost{0.0};
    double safebox_operational_cost{0.0};
    std::string next_payment_date{};
};

/**
 * @brief Synchronized driver account data mapped directly from MongoDB `users`,
 *        `currencies`, `points`, `garages`, and `fleets` in nismara-logistics.
 */
struct DriverAccountProfile {
    AccountSyncStatus status{AccountSyncStatus::Disconnected};
    std::string status_message{"Not connected. Enter your Tracker Token to sync."};
    std::string last_sync_timestamp{};
    bool is_version_outdated{false};
    std::string required_min_version{};

    // Core MongoDB User Identity
    std::string user_id{};
    std::string name{};
    std::string email{};
    std::string image_url{};
    std::string background_url{};
    std::string banner_url{};
    std::string discord_id{};
    int64_t trucky_id{0};
    std::string steam_id{};
    std::string truckersmp_id{};

    // Roles & Ranks
    std::string discord_role{};
    std::string trucky_rank{};
    std::string trucky_rank_color{"#ffffff"};
    std::string trucky_role{};
    std::string trucky_role_color{"#ffffff"};

    // Progression & Economy
    int32_t level{1};
    int64_t xp{0};
    double nc{0.0};
    double total_nc{0.0};
    int32_t points{0}; // Poin penalti pelanggaran (idealnya 0)
    int32_t penalty_points{0};
    int32_t point_price{3000};
    int32_t discount_booster{0};
    int32_t discount_nismara_plus{0};
    int32_t total_discount{0};
    int32_t final_point_price{3000};
    int32_t safebox_tickets{0};
    int32_t max_penalty_tickets{10};
    int32_t joined_convoy{0};

    // Penalty Points Action Toast/Status
    std::string penalty_action_message{};
    bool penalty_action_is_error{false};
    float penalty_action_time_remaining{0.0f};

    // Driver Flags
    bool is_driver{false};
    bool is_on_leave{false};
    bool is_booster{false};
    bool is_guild_member{false};
    bool is_tmp_driver{false};
    bool is_haulmp_driver{false};
    bool is_best_employee_ets2{false};
    bool is_best_employee_ats{false};

    // Insurance Sub-Document
    bool insurance_active{false};
    int32_t insurance_rating{0};
    std::string insurance_started_at{};
    std::string insurance_expired_at{};

    // Nismara+ Sub-Document
    bool nismara_plus_active{false};
    std::string nismara_plus_started_at{};
    std::string nismara_plus_expired_at{};

    // NC Booster Sub-Document
    bool nc_boost_active{false};
    float nc_boost_multiplier{0.0f};
    std::string nc_boost_voucher_title{};
    std::string nc_boost_voucher_code{};
    std::string nc_boost_expired_at{};

    // Top Manager Award Sub-Document
    bool top_manager_active{false};
    std::string top_manager_month{};
    std::string top_manager_expired_at{};

    // Garage & Fleets (Synced from MongoDB `garages` & `fleets`)
    DriverGarageSummary garage{};
    std::vector<DriverFleetVehicle> fleets{};
    int active_fleet_index{-1};
    std::string active_fleet_match_reason{"No vehicle matched"};
    std::string manual_override_fleet_number{};
};

/**
 * @brief Thread-safe snapshot of ETS2/ATS live telemetry and active job state.
 */
struct TelemetrySnapshot {
    // Game & Engine Metadata
    std::string game_id{"unknown"};
    std::string game_name{"Waiting for Simulation..."};
    uint32_t game_version_major{0};
    uint32_t game_version_minor{0};
    bool sdk_active{false};
    bool is_paused{true};
    bool is_driving_in_world{false};
    uint32_t game_time_minutes{0};
    float local_scale{1.0f};

    // Server & Multiplayer Session Detection (Singleplayer / SCS Convoy / TruckersMP / HaulMP)
    std::string server_mode{"Singleplayer"};              // e.g. "Singleplayer", "TruckersMP", "HaulMP", "SCS Convoy"
    std::string server_name{"Offline / Standard Career"}; // e.g. "Simulation 1", "Convoy Session", "HaulMP Server"
    std::string server_detect_source{"Default (No MP Hooks)"};
    std::string server_debug_modules{"None"};
    std::string server_debug_net_summary{"TCP: 0 | UDP: 0"};
    std::string server_debug_last_log{"No [mp] log entry found"};
    std::string server_debug_cmdline{};

    // Truck Identity & Powertrain Metrics
    std::string truck_brand_id{};     // e.g. "renault"
    std::string truck_model_id{};     // e.g. "magnum"
    std::string truck_in_game_id{};   // e.g. "vehicle.renault.magnum"
    std::string truck_brand{};        // e.g. "Renault Trucks"
    std::string truck_name{};         // e.g. "Renault Magnum"
    std::string license_plate{};
    uint32_t truck_wheel_count{4};
    bool electric_enabled{false};
    bool engine_running{false};
    int32_t current_gear{0};
    int32_t displayed_gear{0};
    float speed_mps{0.0f};
    float speed_kmh{0.0f};
    float engine_rpm{0.0f};
    float engine_rpm_limit{2500.0f};
    float fuel_current{0.0f};
    float fuel_capacity{0.0f};
    float fuel_range_km{0.0f};
    float fuel_avg_consumption{0.0f};
    bool fuel_warning{false};
    float odometer_km{0.0f};
    bool parking_brake{false};
    bool motor_brake{false};
    uint32_t retarder_level{0};
    bool light_high_beam{false};
    bool hazard_warning{false};
    bool differential_lock{false};
    float cruise_control_kmh{0.0f};
    float speed_limit_kmh{0.0f};

    // Engine, Fluids & Electrical Diagnostics
    float oil_temperature_c{0.0f};
    float water_temperature_c{0.0f};
    float brake_air_pressure_psi{0.0f};
    float battery_voltage{24.0f};

    // World Space Vector Coordinates & Orientation
    double pos_x{0.0};
    double pos_y{0.0};
    double pos_z{0.0};
    float heading_deg{0.0f};
    float pitch_deg{0.0f};
    float roll_deg{0.0f};

    // Structural Health / Wear Indicators (0.0f = 0%, 1.0f = 100%)
    float wear_engine{0.0f};
    float wear_transmission{0.0f};
    float wear_cabin{0.0f};
    float wear_chassis{0.0f};
    float wear_wheels{0.0f};
    float wear_trailer{0.0f};
    float wear_trailer_chassis{0.0f};
    float wear_trailer_body{0.0f};
    float wear_trailer_wheels{0.0f};
    float cargo_damage{0.0f};
    bool trailer_connected{false};

    // Active Job Contract
    bool job_active{false};
    std::string cargo_id{};
    std::string cargo_name{};
    float cargo_mass_kg{0.0f};
    std::string origin_city{};
    std::string origin_company{};
    std::string destination_city{};
    std::string destination_company{};
    uint32_t contract_deadline_min{0};
    uint64_t reward_payout{0};
    uint32_t planned_distance_km{0};
    float navigation_distance_m{0.0f};
    std::string job_market{};
    bool is_special_job{false};
    std::string last_job_status{"No Active Contract"};

    // Live Job Session Tracking for `nismara-tracker-server` (`jobhistories`)
    std::string active_job_db_id{};         // Auto-incremented jobId ("1", "2", ...) from server
    double active_job_locked_price{1.5};    // Locked cargo price (NC/km)
    bool active_job_is_special{false};      // True if matched VTC Special Contract
    std::string active_job_sync_status{"Ready"};
    float job_start_odometer_km{0.0f};
    int64_t job_start_timestamp_sec{0};
    float job_last_fuel_liters{0.0f};
    float job_fuel_consumed_liters{0.0f};
    float job_max_speed_kmh{0.0f};
    std::vector< std::pair<std::string, int64_t> > job_fines{}; // (offence, amount)

    // Cumulative Job Damage Tracking (0.0f = 0%, 1.0f = 100%)
    // Preserves and accumulates all damage taken during the job even if repaired mid-trip!
    float job_prev_wear_engine{0.0f};
    float job_prev_wear_transmission{0.0f};
    float job_prev_wear_cabin{0.0f};
    float job_prev_wear_chassis{0.0f};
    float job_prev_wear_wheels{0.0f};
    float job_prev_wear_trailer_chassis{0.0f};
    float job_prev_wear_trailer_body{0.0f};
    float job_prev_wear_trailer_wheels{0.0f};
    float job_prev_cargo_damage{0.0f};

    float job_accum_wear_engine{0.0f};
    float job_accum_wear_transmission{0.0f};
    float job_accum_wear_cabin{0.0f};
    float job_accum_wear_chassis{0.0f};
    float job_accum_wear_wheels{0.0f};
    float job_accum_wear_trailer_chassis{0.0f};
    float job_accum_wear_trailer_body{0.0f};
    float job_accum_wear_trailer_wheels{0.0f};
    float job_accum_vehicle_damage{0.0f};
    float job_accum_trailer_damage{0.0f};
    float job_accum_cargo_damage{0.0f};

    // Last Completed Job Paycheck Receipt (from POST /api/tracker/jobs/complete)
    bool has_last_job_receipt{false};
    std::string last_receipt_job_id{};
    std::string last_receipt_game_mode{};
    int32_t last_receipt_distance_km{0};
    int32_t last_receipt_revenue_nc{0};
    int32_t last_receipt_base_nc{0};
    int32_t last_receipt_mp_bonus_nc{0};
    int32_t last_receipt_hardcore_nc{0};
    int32_t last_receipt_nplus_nc{0};
    int32_t last_receipt_cost_nc{0};
    int32_t last_receipt_tax_nc{0};
    int32_t last_receipt_xp{0};
    int32_t last_receipt_penalty_pts{0};

    // Hardcore Mode Real-time Tracking & Criteria
    bool hardcore_eligible{true};
    bool hardcore_eligible_at_start{true};
    bool hardcore_disqualified{false};
    std::string hardcore_disqualify_reason{};
    float hardcore_speeding_seconds{0.0f};
    int32_t hardcore_rest_overdue_minutes{0};
    bool hardcore_auto_park_used{false};
    bool hardcore_warp_detected{false};
    float hardcore_last_wallclock_sec{0.0f};
    uint32_t hardcore_last_game_time_min{0};
    float hardcore_last_cvar_check_time{0.0f};

    // Hardcore Profile Diagnostic Status (9 Parameters)
    bool hardcore_profile_scanned{false};
    bool hc_traffic_ok{false};
    bool hc_fatigue_ok{false};
    bool hc_police_ok{false};
    bool hc_parking_ok{false};
    bool hc_road_events_ok{false};
    bool hc_rain_prob_ok{false};
    bool hc_advanced_coupling_ok{false};
    bool hc_fuel_simulation_ok{false};
    bool hc_air_brake_ok{false};
    bool hc_forbidden_mods_found{false};
    std::string hc_forbidden_mods_list{};

    // Last Job Hardcore Evaluation Receipt
    bool last_receipt_is_hardcore{false};
    double last_receipt_hardcore_points{0.0};
    int32_t last_receipt_stars_distance{0};
    int32_t last_receipt_stars_mass{0};
    int32_t last_receipt_stars_damage{0};
    int32_t last_receipt_stars_fuel{0};
    int32_t last_receipt_stars_fines{0};
    int32_t last_receipt_stars_total{0};
    std::string last_receipt_disqualify_reason{};
};

/**
 * @brief Thread-safe telemetry and driver account store isolating SCS SDK callbacks,
 *        DX11 rendering, and background WinHTTP network workers.
 */
class TelemetryState {
public:
    static TelemetryState& Instance();

    TelemetryState(const TelemetryState&) = delete;
    TelemetryState& operator=(const TelemetryState&) = delete;

    /**
     * @brief Acquires a shared lock (reader) and returns an immutable copy of the telemetry state.
     */
    TelemetrySnapshot GetSnapshot() const;

    /**
     * @brief Checks if the driver is currently in an active truck cabin driving session
     *        (SDK active, not paused, not in main menu, not in loading screen, truck loaded).
     */
    bool IsInCabinDrivingSession() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return snapshot_.sdk_active &&
               !snapshot_.is_paused &&
               snapshot_.is_driving_in_world &&
               (!snapshot_.truck_in_game_id.empty() || !snapshot_.truck_name.empty());
    }

    /**
     * @brief Acquires an exclusive lock (writer) and applies mutations from SCS SDK callbacks.
     */
    template <typename UpdaterFn>
    void Write(UpdaterFn&& updater) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        std::forward<UpdaterFn>(updater)(snapshot_);
    }

    /**
     * @brief Acquires a shared lock (reader) and returns an immutable copy of the synced driver profile.
     */
    DriverAccountProfile GetDriverAccount() const;

    /**
     * @brief Acquires an exclusive lock (writer) and updates the synced driver profile from network workers.
     */
    template <typename UpdaterFn>
    void WriteDriverAccount(UpdaterFn&& updater) {
        std::unique_lock<std::shared_mutex> lock(account_mutex_);
        std::forward<UpdaterFn>(updater)(driver_account_);
    }

    /**
     * @brief Matches the currently driven in-game vehicle against the driver's synced MongoDB `fleets`
     *        (e.g., matching "vehicle.renault.magnum" -> "NL-074").
     */
    void MatchActiveFleet();

    /**
     * @brief Sets or clears a manual fleet number override (e.g., for mod trucks or duplicate models).
     */
    void SetManualFleetOverride(const std::string& fleet_number);

    /**
     * @brief Resets telemetry state back to default values upon SDK shutdown.
     */
    void Reset();

private:
    TelemetryState() = default;
    ~TelemetryState() = default;

    mutable std::shared_mutex mutex_;
    TelemetrySnapshot snapshot_{};

    mutable std::shared_mutex account_mutex_;
    DriverAccountProfile driver_account_{};
};

} // namespace nismara::core
