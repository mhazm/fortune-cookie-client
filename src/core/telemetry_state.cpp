#include "core/telemetry_state.hpp"

#include <algorithm>
#include <cctype>

namespace nismara::core {

namespace {

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string StripVehiclePrefix(const std::string& id) {
    const std::string lower = ToLowerAscii(id);
    if (lower.rfind("vehicle.", 0) == 0) {
        return lower.substr(8);
    }
    return lower;
}

bool ContainsSubstringCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.empty()) {
        return false;
    }
    const std::string h = ToLowerAscii(haystack);
    const std::string n = ToLowerAscii(needle);
    return h.find(n) != std::string::npos;
}

bool IsFleetAtsVehicle(const DriverFleetVehicle& f) {
    const std::string gid = ToLowerAscii(f.game_id);
    if (gid == "2" || gid == "ats" || gid == "amtrucks") {
        return true;
    }
    const std::string ig = ToLowerAscii(f.in_game_id);
    if (ig.find("westernstar") != std::string::npos ||
        ig.find("mack.") != std::string::npos ||
        ig.find("kenworth") != std::string::npos ||
        ig.find("peterbilt") != std::string::npos ||
        ig.find("freightliner") != std::string::npos ||
        ig.find("international") != std::string::npos ||
        ig.find("volvo.vnl") != std::string::npos ||
        ig.find(".ats") != std::string::npos) {
        return true;
    }
    return false;
}

bool IsFleetMatchingGame(const DriverFleetVehicle& f, const std::string& active_game_id) {
    const std::string g = ToLowerAscii(active_game_id);
    const bool is_ats_session = (g == "ats" || g == "amtrucks" || g == "2");
    return IsFleetAtsVehicle(f) == is_ats_session;
}

} // namespace

TelemetryState& TelemetryState::Instance() {
    static TelemetryState instance;
    return instance;
}

TelemetrySnapshot TelemetryState::GetSnapshot() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return snapshot_;
}

DriverAccountProfile TelemetryState::GetDriverAccount() const {
    std::shared_lock<std::shared_mutex> lock(account_mutex_);
    return driver_account_;
}

void TelemetryState::SetManualFleetOverride(const std::string& fleet_number) {
    {
        std::unique_lock<std::shared_mutex> lock(account_mutex_);
        driver_account_.manual_override_fleet_number = fleet_number;
    }
    MatchActiveFleet();
}

void TelemetryState::MatchActiveFleet() {
    const TelemetrySnapshot snap = GetSnapshot();

    std::unique_lock<std::shared_mutex> lock(account_mutex_);
    auto& acc = driver_account_;

    if (acc.fleets.empty()) {
        acc.active_fleet_index = -1;
        acc.active_fleet_match_reason = "No fleets in garage";
        return;
    }

    // 0. Check manual override first (useful for mod trucks or when owning multiple identical models)
    if (!acc.manual_override_fleet_number.empty()) {
        for (size_t i = 0; i < acc.fleets.size(); ++i) {
            if (acc.fleets[i].fleet_number == acc.manual_override_fleet_number) {
                acc.active_fleet_index = static_cast<int>(i);
                acc.active_fleet_match_reason = "Manual Selection (" + acc.fleets[i].fleet_number + ")";
                return;
            }
        }
    }

    if (snap.truck_in_game_id.empty() && snap.truck_brand.empty() && snap.truck_name.empty()) {
        acc.active_fleet_index = -1;
        acc.active_fleet_match_reason = "Waiting for in-game vehicle";
        return;
    }

    const std::string in_game_canonical = ToLowerAscii(snap.truck_in_game_id);
    const std::string in_game_short = StripVehiclePrefix(snap.truck_in_game_id);
    const std::string in_game_full_title = ToLowerAscii(snap.truck_brand + " " + snap.truck_name);

    int best_idx = -1;
    int best_score = 0;
    std::string best_reason = "Unregistered vehicle in garage";

    for (size_t i = 0; i < acc.fleets.size(); ++i) {
        const auto& f = acc.fleets[i];
        if (!IsFleetMatchingGame(f, snap.game_id)) {
            continue;
        }
        int score = 0;
        std::string reason;

        const std::string db_in_game_id = ToLowerAscii(f.in_game_id);
        const std::string db_short_id = StripVehiclePrefix(f.in_game_id);

        // Tier 1: Exact in_game_id match (e.g. "vehicle.renault.magnum" == "vehicle.renault.magnum")
        if (!in_game_canonical.empty() && !db_in_game_id.empty()) {
            if (in_game_canonical == db_in_game_id || (!in_game_short.empty() && in_game_short == db_short_id)) {
                score = 1000;
                reason = "Auto-Matched via ID (" + f.fleet_number + ")";
            }
        }

        // Tier 2: Brand + Model Name match (fallback if in_game_id format varies)
        if (score == 0 && !f.model_name.empty()) {
            const bool model_match =
                ContainsSubstringCaseInsensitive(snap.truck_name, f.model_name) ||
                ContainsSubstringCaseInsensitive(in_game_full_title, f.model_name) ||
                (!snap.truck_model_id.empty() && ContainsSubstringCaseInsensitive(f.model_name, snap.truck_model_id));

            const bool brand_match =
                f.brand_name.empty() ||
                ContainsSubstringCaseInsensitive(snap.truck_brand, f.brand_name) ||
                ContainsSubstringCaseInsensitive(snap.truck_brand_id, f.brand_name) ||
                ContainsSubstringCaseInsensitive(f.fleet_name, snap.truck_brand_id);

            if (model_match && brand_match) {
                score = 500;
                reason = "Auto-Matched via Model (" + f.fleet_number + ")";
            }
        }

        // Tier 3: Fleet Name match against in-game truck name
        if (score == 0 && !snap.truck_name.empty() && ContainsSubstringCaseInsensitive(f.fleet_name, snap.truck_name)) {
            score = 300;
            reason = "Auto-Matched via Fleet Name (" + f.fleet_number + ")";
        }

        if (score > 0) {
            // Tie-breakers when driver owns multiple trucks of the same model:
            if (f.status == "active") {
                score += 20;
            }
            if (f.is_assigned_driver) {
                score += 10;
            }

            if (score > best_score) {
                best_score = score;
                best_idx = static_cast<int>(i);
                best_reason = reason;
            }
        }
    }

    acc.active_fleet_index = best_idx;
    acc.active_fleet_match_reason = best_reason;
}

void TelemetryState::Reset() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    snapshot_ = TelemetrySnapshot{};
}

} // namespace nismara::core
