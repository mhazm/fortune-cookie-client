#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nismara::core {

struct HardcoreProfileStatus {
    bool scanned{false};

    // 9 Required Profile & Game Settings
    bool traffic_ok{false};            // g_traffic >= 1.0
    bool fatigue_ok{false};            // g_fatigue == 1
    bool police_ok{false};             // g_police == 1
    bool parking_ok{false};            // g_parking_difficulty == 2 (Skilled) || g_simple_parking_doubles == 0
    bool road_events_ok{false};        // g_road_events >= 0.10
    bool rain_prob_ok{false};          // g_bad_weather_factor >= 0.10 || g_rain_prob >= 0.10
    bool advanced_coupling_ok{false};  // g_trailer_advanced_coupling == 1
    bool fuel_simulation_ok{false};    // g_fuel_simulation == 1
    bool air_brake_ok{false};          // g_hardcore_simulation == 1 (Air Brakes Simulation)

    // Raw Values parsed from game.log.txt / config.cfg
    float traffic_val{1.0f};
    int fatigue_val{0};
    int police_val{0};
    int parking_val{0};
    float road_events_val{0.0f};
    float rain_prob_val{0.0f};
    int advanced_coupling_val{0};
    int fuel_simulation_val{0};
    int air_brake_val{0};

    // Forbidden Mods Detection
    bool has_forbidden_mods{false};
    std::vector<std::string> forbidden_mods{};

    bool AllSettingsValid() const {
        return traffic_ok && fatigue_ok && police_ok && parking_ok &&
               road_events_ok && rain_prob_ok && advanced_coupling_ok &&
               fuel_simulation_ok && air_brake_ok && !has_forbidden_mods;
    }

    std::string GetSummaryReason() const {
        if (has_forbidden_mods) return "Mod Terlarang Aktif";
        if (!traffic_ok) return "Traffic Game Mati (g_traffic < 1)";
        if (!fatigue_ok) return "Simulasi Kelelahan Mati (g_fatigue = 0)";
        if (!police_ok) return "Tilang Polisi Mati (g_police = 0)";
        if (!parking_ok) return "Kesulitan Parkir Belum Skilled";
        if (!road_events_ok) return "Road Events < 10%";
        if (!rain_prob_ok) return "Probabilitas Hujan < 10%";
        if (!advanced_coupling_ok) return "Kopling Gandengan Lanjutan Mati";
        if (!fuel_simulation_ok) return "Konsumsi BBM Realistis Mati";
        if (!air_brake_ok) return "Simulasi Rem Angin Mati";
        return "Semua Parameter Hardcore Terpenuhi";
    }
};

class ProfileValidator {
public:
    /**
     * @brief Scans game documents directory (Documents/<Game>/) for config.cfg and game.log.txt
     *        to validate the 9 hardcore simulation cvars and identify any forbidden mods.
     */
    static HardcoreProfileStatus ScanProfileSettings(const std::filesystem::path& game_dir);
};

} // namespace nismara::core

