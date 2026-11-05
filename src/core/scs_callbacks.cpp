#include "scs/scs-telemetry.h"
#include "core/api_client.hpp"
#include "core/companion_gate.hpp"
#include "core/config_manager.hpp"
#include "core/server_detector.hpp"
#include "core/telemetry_state.hpp"
#include "hooks/dx11_hook.hpp"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

scs_log_t g_game_log = nullptr;

void LogToGame(const scs_log_type_t type, const char* format, ...) {
    if (!g_game_log) {
        return;
    }
    char buffer[1024] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    g_game_log(type, buffer);
}

const scs_named_value_t* FindNamedAttr(
    const scs_named_value_t* attributes,
    const char* name,
    const scs_u32_t index,
    const scs_value_type_t expected_type) {
    if (!attributes) {
        return nullptr;
    }
    for (const scs_named_value_t* current = attributes; current->name != nullptr; ++current) {
        if (current->index == index && std::strcmp(current->name, name) == 0) {
            if (current->value.type == expected_type) {
                return current;
            }
            return nullptr;
        }
    }
    return nullptr;
}

const scs_named_value_t* FindAttribute(
    const scs_telemetry_configuration_t& config,
    const char* name,
    const scs_u32_t index,
    const scs_value_type_t expected_type) {
    return FindNamedAttr(config.attributes, name, index, expected_type);
}

std::string GetStringAttribute(
    const scs_telemetry_configuration_t& config,
    const char* name,
    const char* fallback = "") {
    const auto* attr = FindAttribute(config, name, SCS_U32_NIL, SCS_VALUE_TYPE_string);
    if (attr && attr->value.value_string.value) {
        return attr->value.value_string.value;
    }
    return fallback;
}

float GetFloatAttribute(
    const scs_telemetry_configuration_t& config,
    const char* name,
    float fallback = 0.0f) {
    const auto* attr = FindAttribute(config, name, SCS_U32_NIL, SCS_VALUE_TYPE_float);
    return attr ? attr->value.value_float.value : fallback;
}

uint32_t GetU32Attribute(
    const scs_telemetry_configuration_t& config,
    const char* name,
    uint32_t fallback = 0) {
    const auto* attr = FindAttribute(config, name, SCS_U32_NIL, SCS_VALUE_TYPE_u32);
    return attr ? attr->value.value_u32.value : fallback;
}

bool GetBoolAttribute(
    const scs_named_value_t* attributes,
    const char* name,
    bool fallback = false) {
    const auto* attr = FindNamedAttr(attributes, name, SCS_U32_NIL, SCS_VALUE_TYPE_bool);
    return attr ? (attr->value.value_bool.value != 0) : fallback;
}

int64_t GetS64Attribute(
    const scs_named_value_t* attributes,
    const char* name,
    int64_t fallback = 0) {
    const auto* attr = FindNamedAttr(attributes, name, SCS_U32_NIL, SCS_VALUE_TYPE_s64);
    if (attr) {
        return attr->value.value_s64.value;
    }
    const auto* attr_u64 = FindNamedAttr(attributes, name, SCS_U32_NIL, SCS_VALUE_TYPE_u64);
    if (attr_u64) {
        return static_cast<int64_t>(attr_u64->value.value_u64.value);
    }
    return fallback;
}

uint64_t GetU64Attribute(
    const scs_telemetry_configuration_t& config,
    const char* name,
    uint64_t fallback = 0) {
    const auto* attr = FindAttribute(config, name, SCS_U32_NIL, SCS_VALUE_TYPE_u64);
    if (attr) {
        return attr->value.value_u64.value;
    }
    const auto* attr_s64 = FindAttribute(config, name, SCS_U32_NIL, SCS_VALUE_TYPE_s64);
    if (attr_s64 && attr_s64->value.value_s64.value > 0) {
        return static_cast<uint64_t>(attr_s64->value.value_s64.value);
    }
    return fallback;
}

std::string MapServerModeToGameMode(const std::string& server_mode) {
    if (server_mode == "TruckersMP") {
        return "truckersmp";
    }
    if (server_mode == "HaulMP") {
        return "haulmp";
    }
    return "sp";
}

// ============================================================================
// Periodic Channel Callbacks
// ============================================================================

SCSAPI_VOID OnChannelSpeed(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float mps = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([mps](nismara::core::TelemetrySnapshot& s) {
        s.speed_mps = mps;
        s.speed_kmh = std::fabs(mps) * 3.6f;
        if (s.job_active && s.speed_kmh > s.job_max_speed_kmh) {
            s.job_max_speed_kmh = s.speed_kmh;
        }
    });
}

SCSAPI_VOID OnChannelRpm(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float rpm = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([rpm](nismara::core::TelemetrySnapshot& s) {
        s.engine_rpm = rpm;
    });
}

SCSAPI_VOID OnChannelEngineGear(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const int32_t gear = (value && value->type == SCS_VALUE_TYPE_s32) ? value->value_s32.value : 0;
    nismara::core::TelemetryState::Instance().Write([gear](nismara::core::TelemetrySnapshot& s) {
        s.current_gear = gear;
    });
}

SCSAPI_VOID OnChannelDisplayedGear(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const int32_t gear = (value && value->type == SCS_VALUE_TYPE_s32) ? value->value_s32.value : 0;
    nismara::core::TelemetryState::Instance().Write([gear](nismara::core::TelemetrySnapshot& s) {
        s.displayed_gear = gear;
    });
}

SCSAPI_VOID OnChannelEngineEnabled(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool enabled = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([enabled](nismara::core::TelemetrySnapshot& s) {
        s.engine_running = enabled;
    });
}

SCSAPI_VOID OnChannelElectricEnabled(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool enabled = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([enabled](nismara::core::TelemetrySnapshot& s) {
        s.electric_enabled = enabled;
    });
}

SCSAPI_VOID OnChannelFuelAmount(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float liters = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([liters](nismara::core::TelemetrySnapshot& s) {
        if (s.job_active && s.job_last_fuel_liters > 0.0f && liters < s.job_last_fuel_liters) {
            const float delta = s.job_last_fuel_liters - liters;
            if (delta < 50.0f) { // Ignore truck switch jumps
                s.job_fuel_consumed_liters += delta;
            }
        }
        s.job_last_fuel_liters = liters;
        s.fuel_current = liters;
    });
}

SCSAPI_VOID OnChannelFuelRange(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float range_km = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([range_km](nismara::core::TelemetrySnapshot& s) {
        s.fuel_range_km = range_km;
    });
}

SCSAPI_VOID OnChannelOdometer(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float odo = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([odo](nismara::core::TelemetrySnapshot& s) {
        if (s.job_active && s.job_start_odometer_km <= 0.0f && odo > 0.0f) {
            s.job_start_odometer_km = odo;
        }
        s.odometer_km = odo;
    });
}

SCSAPI_VOID OnChannelParkingBrake(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool active = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([active](nismara::core::TelemetrySnapshot& s) {
        s.parking_brake = active;
    });
}

SCSAPI_VOID OnChannelCruiseControl(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float cc_mps = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([cc_mps](nismara::core::TelemetrySnapshot& s) {
        s.cruise_control_kmh = std::fabs(cc_mps) * 3.6f;
    });
}

SCSAPI_VOID OnChannelMotorBrake(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool active = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([active](nismara::core::TelemetrySnapshot& s) {
        s.motor_brake = active;
    });
}

SCSAPI_VOID OnChannelRetarder(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const uint32_t level = (value && value->type == SCS_VALUE_TYPE_u32) ? value->value_u32.value : 0;
    nismara::core::TelemetryState::Instance().Write([level](nismara::core::TelemetrySnapshot& s) {
        s.retarder_level = level;
    });
}

SCSAPI_VOID OnChannelHighBeam(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool active = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([active](nismara::core::TelemetrySnapshot& s) {
        s.light_high_beam = active;
    });
}

SCSAPI_VOID OnChannelHazard(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool active = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([active](nismara::core::TelemetrySnapshot& s) {
        s.hazard_warning = active;
    });
}

SCSAPI_VOID OnChannelDiffLock(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool active = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([active](nismara::core::TelemetrySnapshot& s) {
        s.differential_lock = active;
    });
}

SCSAPI_VOID OnChannelFuelWarning(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool active = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([active](nismara::core::TelemetrySnapshot& s) {
        s.fuel_warning = active;
    });
}

SCSAPI_VOID OnChannelFuelAvgConsumption(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    // SCS fuel consumption is in liters per meter. Convert to L/100km: val * 100000.0f
    const float val = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    const float l_per_100km = (val > 0.0f) ? (val * 100000.0f) : 0.0f;
    nismara::core::TelemetryState::Instance().Write([l_per_100km](nismara::core::TelemetrySnapshot& s) {
        s.fuel_avg_consumption = l_per_100km;
    });
}

SCSAPI_VOID OnChannelSpeedLimit(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float limit_mps = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([limit_mps](nismara::core::TelemetrySnapshot& s) {
        s.speed_limit_kmh = std::fabs(limit_mps) * 3.6f;
    });
}

SCSAPI_VOID OnChannelOilTemp(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float temp_c = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([temp_c](nismara::core::TelemetrySnapshot& s) {
        s.oil_temperature_c = temp_c;
    });
}

SCSAPI_VOID OnChannelWaterTemp(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float temp_c = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([temp_c](nismara::core::TelemetrySnapshot& s) {
        s.water_temperature_c = temp_c;
    });
}

SCSAPI_VOID OnChannelBrakeAirPressure(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    // SCS brake air pressure is in psi (or bar depending on game config)
    const float psi = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([psi](nismara::core::TelemetrySnapshot& s) {
        s.brake_air_pressure_psi = psi;
    });
}

SCSAPI_VOID OnChannelBatteryVoltage(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float v = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 24.0f;
    nismara::core::TelemetryState::Instance().Write([v](nismara::core::TelemetrySnapshot& s) {
        s.battery_voltage = v;
    });
}

SCSAPI_VOID OnChannelWorldPlacement(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    if (!value || value->type != SCS_VALUE_TYPE_dplacement) {
        return;
    }

    const scs_value_dplacement_t placement = value->value_dplacement;
    // SCS euler heading is normalized in rotations: <0.0, 1.0) counterclockwise from North
    float heading_deg = std::fmod(360.0f - (placement.orientation.heading * 360.0f), 360.0f);
    if (heading_deg < 0.0f) {
        heading_deg += 360.0f;
    }
    const float pitch_deg = placement.orientation.pitch * 360.0f;
    const float roll_deg = placement.orientation.roll * 360.0f;

    nismara::core::TelemetryState::Instance().Write(
        [placement, heading_deg, pitch_deg, roll_deg](nismara::core::TelemetrySnapshot& s) {
            s.pos_x = placement.position.x;
            s.pos_y = placement.position.y;
            s.pos_z = placement.position.z;
            s.heading_deg = heading_deg;
            s.pitch_deg = pitch_deg;
            s.roll_deg = roll_deg;
        });
}

SCSAPI_VOID OnChannelWear(
    const scs_string_t name,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    if (!name) {
        return;
    }
    const float wear = (value && value->type == SCS_VALUE_TYPE_float)
                           ? std::clamp(value->value_float.value, 0.0f, 1.0f)
                           : 0.0f;

    nismara::core::TelemetryState::Instance().Write([name, wear](nismara::core::TelemetrySnapshot& s) {
        auto update_cumulative = [&s](float current_w, float& prev_w, float& accum_w) {
            if (s.job_active) {
                if (current_w > prev_w) {
                    accum_w = std::clamp(accum_w + (current_w - prev_w), 0.0f, 1.0f);
                }
            }
            prev_w = current_w;
        };

        if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_wear_cabin) == 0) {
            s.wear_cabin = wear;
            update_cumulative(wear, s.job_prev_wear_cabin, s.job_accum_wear_cabin);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_wear_chassis) == 0) {
            s.wear_chassis = wear;
            update_cumulative(wear, s.job_prev_wear_chassis, s.job_accum_wear_chassis);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_wear_wheels) == 0) {
            s.wear_wheels = wear;
            update_cumulative(wear, s.job_prev_wear_wheels, s.job_accum_wear_wheels);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_wear_engine) == 0) {
            s.wear_engine = wear;
            update_cumulative(wear, s.job_prev_wear_engine, s.job_accum_wear_engine);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_wear_transmission) == 0) {
            s.wear_transmission = wear;
            update_cumulative(wear, s.job_prev_wear_transmission, s.job_accum_wear_transmission);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRAILER_CHANNEL_wear_chassis) == 0) {
            s.wear_trailer_chassis = wear;
            s.wear_trailer = std::max({s.wear_trailer_chassis, s.wear_trailer_body, s.wear_trailer_wheels});
            update_cumulative(wear, s.job_prev_wear_trailer_chassis, s.job_accum_wear_trailer_chassis);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRAILER_CHANNEL_wear_body) == 0) {
            s.wear_trailer_body = wear;
            s.wear_trailer = std::max({s.wear_trailer_chassis, s.wear_trailer_body, s.wear_trailer_wheels});
            update_cumulative(wear, s.job_prev_wear_trailer_body, s.job_accum_wear_trailer_body);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRAILER_CHANNEL_wear_wheels) == 0) {
            s.wear_trailer_wheels = wear;
            s.wear_trailer = std::max({s.wear_trailer_chassis, s.wear_trailer_body, s.wear_trailer_wheels});
            update_cumulative(wear, s.job_prev_wear_trailer_wheels, s.job_accum_wear_trailer_wheels);
        } else if (std::strcmp(name, SCS_TELEMETRY_TRAILER_CHANNEL_cargo_damage) == 0 ||
                   std::strcmp(name, SCS_TELEMETRY_JOB_CHANNEL_cargo_damage) == 0) {
            s.cargo_damage = wear;
            update_cumulative(wear, s.job_prev_cargo_damage, s.job_accum_cargo_damage);
        }

        if (s.job_active) {
            s.job_accum_vehicle_damage = std::max({
                s.job_accum_wear_engine,
                s.job_accum_wear_transmission,
                s.job_accum_wear_cabin,
                s.job_accum_wear_chassis,
                s.job_accum_wear_wheels
            });
            s.job_accum_trailer_damage = std::max({
                s.job_accum_wear_trailer_chassis,
                s.job_accum_wear_trailer_body,
                s.job_accum_wear_trailer_wheels
            });
        }
    });
}

SCSAPI_VOID OnChannelTrailerConnected(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const bool connected = (value && value->type == SCS_VALUE_TYPE_bool) ? (value->value_bool.value != 0) : false;
    nismara::core::TelemetryState::Instance().Write([connected](nismara::core::TelemetrySnapshot& s) {
        s.trailer_connected = connected;
        if (!connected) {
            s.wear_trailer = 0.0f;
            s.wear_trailer_chassis = 0.0f;
            s.wear_trailer_body = 0.0f;
            s.wear_trailer_wheels = 0.0f;
            s.job_prev_wear_trailer_chassis = 0.0f;
            s.job_prev_wear_trailer_body = 0.0f;
            s.job_prev_wear_trailer_wheels = 0.0f;
        }
    });
}

SCSAPI_VOID OnChannelGameTime(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const uint32_t game_time = (value && value->type == SCS_VALUE_TYPE_u32) ? value->value_u32.value : 0;
    nismara::core::TelemetryState::Instance().Write([game_time](nismara::core::TelemetrySnapshot& s) {
        s.game_time_minutes = game_time;
    });
}

SCSAPI_VOID OnChannelNavigationDistance(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float nav_dist_m = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
    nismara::core::TelemetryState::Instance().Write([nav_dist_m](nismara::core::TelemetrySnapshot& s) {
        s.navigation_distance_m = std::max(0.0f, nav_dist_m);
    });
}

SCSAPI_VOID OnChannelLocalScale(
    const scs_string_t /*name*/,
    const scs_u32_t /*index*/,
    const scs_value_t* const value,
    const scs_context_t /*context*/) {
    const float scale = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 1.0f;
    nismara::core::TelemetryState::Instance().Write([scale](nismara::core::TelemetrySnapshot& s) {
        s.local_scale = scale;
    });
}

// ============================================================================
// Telemetry Event Callbacks
// ============================================================================

SCSAPI_VOID OnTelemetryPause(
    const scs_event_t event,
    const void* const /*event_info*/,
    const scs_context_t /*context*/) {
    const bool paused = (event == SCS_TELEMETRY_EVENT_paused);
    nismara::core::TelemetryState::Instance().Write([paused](nismara::core::TelemetrySnapshot& s) {
        s.is_paused = paused;
    });
}

SCSAPI_VOID OnTelemetryConfiguration(
    const scs_event_t /*event*/,
    const void* const event_info,
    const scs_context_t /*context*/) {
    if (!event_info) {
        return;
    }

    const auto* info = static_cast<const scs_telemetry_configuration_t*>(event_info);
    if (!info->id) {
        return;
    }

    if (std::strcmp(info->id, SCS_TELEMETRY_CONFIG_truck) == 0) {
        const std::string brand_id = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand_id);
        const std::string model_id = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_id);
        const std::string brand = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand);
        const std::string name = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_name);
        const std::string plate = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_license_plate);
        const uint32_t wheels = GetU32Attribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_count, 4);
        const float fuel_cap = GetFloatAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_fuel_capacity, 0.0f);
        const float rpm_lim = GetFloatAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_rpm_limit, 2500.0f);

        std::string canonical_in_game_id;
        if (!model_id.empty()) {
            if (model_id.rfind("vehicle.", 0) == 0) {
                canonical_in_game_id = model_id;
            } else if (!brand_id.empty() && model_id.rfind(brand_id + ".", 0) == 0) {
                canonical_in_game_id = "vehicle." + model_id;
            } else if (!brand_id.empty()) {
                canonical_in_game_id = "vehicle." + brand_id + "." + model_id;
            } else {
                canonical_in_game_id = model_id;
            }
        }

        nismara::core::TelemetryState::Instance().Write([&](nismara::core::TelemetrySnapshot& s) {
            s.truck_brand_id = brand_id;
            s.truck_model_id = model_id;
            s.truck_in_game_id = canonical_in_game_id;
            s.truck_brand = brand;
            s.truck_name = name;
            s.license_plate = plate;
            s.truck_wheel_count = wheels;
            s.fuel_capacity = fuel_cap;
            s.engine_rpm_limit = (rpm_lim > 100.0f) ? rpm_lim : 2500.0f;
        });

        nismara::core::TelemetryState::Instance().MatchActiveFleet();

        LogToGame(
            SCS_LOG_TYPE_message,
            "[NismaraTracker] Truck config loaded: %s %s (in_game_id=%s)",
            brand.c_str(),
            name.c_str(),
            canonical_in_game_id.c_str());
    } else if (std::strcmp(info->id, SCS_TELEMETRY_CONFIG_job) == 0 ||
               std::strcmp(info->id, SCS_TELEMETRY_CONFIG_car_job) == 0) {
        const bool has_attributes = (info->attributes != nullptr && info->attributes[0].name != nullptr);
        if (!has_attributes) {
            nismara::core::TelemetryState::Instance().Write([](nismara::core::TelemetrySnapshot& s) {
                s.job_active = false;
                s.cargo_id.clear();
                s.cargo_name.clear();
                s.cargo_mass_kg = 0.0f;
                s.origin_city.clear();
                s.origin_company.clear();
                s.destination_city.clear();
                s.destination_company.clear();
                s.contract_deadline_min = 0;
                s.reward_payout = 0;
                s.planned_distance_km = 0;
                s.job_market.clear();
                s.is_special_job = false;
            });
            return;
        }

        const std::string cargo_id = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_cargo_id, "unknown_cargo");
        const std::string cargo = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_cargo, "Unknown Cargo");
        const float mass = GetFloatAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_cargo_mass, 0.0f);
        const std::string src_city = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_source_city, "N/A");
        const std::string src_comp = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_source_company, "");
        const std::string dst_city = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_destination_city, "N/A");
        const std::string dst_comp = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_destination_company, "");
        const uint32_t deadline = GetU32Attribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_delivery_time, 0);
        const uint64_t income = GetU64Attribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_income, 0);
        const uint32_t planned_km = GetU32Attribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_planned_distance_km, 0);
        const bool special_job = GetBoolAttribute(info->attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_special_job, false);
        std::string market = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_job_market, "");
        if (market.empty()) {
            market = GetStringAttribute(*info, SCS_TELEMETRY_CONFIG_ATTRIBUTE_car_job_market, "standard");
        }

        bool should_post_start = false;
        nismara::core::JobStartRequest start_req{};

        const auto acc = nismara::core::TelemetryState::Instance().GetDriverAccount();
        std::string matched_fleet_number;
        if (acc.active_fleet_index >= 0 && acc.active_fleet_index < static_cast<int>(acc.fleets.size())) {
            matched_fleet_number = acc.fleets[acc.active_fleet_index].fleet_number;
        }

        nismara::core::TelemetryState::Instance().Write([&](nismara::core::TelemetrySnapshot& s) {
            const bool is_new_contract =
                !s.job_active ||
                s.cargo_id != cargo_id ||
                s.origin_city != src_city ||
                s.destination_city != dst_city;

            s.job_active = true;
            s.cargo_id = cargo_id;
            s.cargo_name = cargo;
            s.cargo_mass_kg = mass;
            s.origin_city = src_city;
            s.origin_company = src_comp;
            s.destination_city = dst_city;
            s.destination_company = dst_comp;
            s.contract_deadline_min = deadline;
            s.reward_payout = income;
            s.planned_distance_km = planned_km;
            s.job_market = market;
            s.is_special_job = special_job;
            s.last_job_status = "Active Contract (job_started)";

            if (is_new_contract) {
                s.job_start_odometer_km = s.odometer_km;
                s.job_start_timestamp_sec = static_cast<int64_t>(std::time(nullptr));
                s.job_last_fuel_liters = s.fuel_current;
                s.job_fuel_consumed_liters = 0.0f;
                s.job_max_speed_kmh = s.speed_kmh;
                s.job_fines.clear();

                // Initialize cumulative damage baseline from current wear at job start
                s.job_prev_wear_engine = s.wear_engine;
                s.job_prev_wear_transmission = s.wear_transmission;
                s.job_prev_wear_cabin = s.wear_cabin;
                s.job_prev_wear_chassis = s.wear_chassis;
                s.job_prev_wear_wheels = s.wear_wheels;
                s.job_prev_wear_trailer_chassis = s.wear_trailer_chassis;
                s.job_prev_wear_trailer_body = s.wear_trailer_body;
                s.job_prev_wear_trailer_wheels = s.wear_trailer_wheels;
                s.job_prev_cargo_damage = s.cargo_damage;

                s.job_accum_wear_engine = s.wear_engine;
                s.job_accum_wear_transmission = s.wear_transmission;
                s.job_accum_wear_cabin = s.wear_cabin;
                s.job_accum_wear_chassis = s.wear_chassis;
                s.job_accum_wear_wheels = s.wear_wheels;
                s.job_accum_wear_trailer_chassis = s.wear_trailer_chassis;
                s.job_accum_wear_trailer_body = s.wear_trailer_body;
                s.job_accum_wear_trailer_wheels = s.wear_trailer_wheels;
                s.job_accum_vehicle_damage = std::max({
                    s.wear_engine,
                    s.wear_transmission,
                    s.wear_cabin,
                    s.wear_chassis,
                    s.wear_wheels
                });
                s.job_accum_trailer_damage = s.wear_trailer;
                s.job_accum_cargo_damage = s.cargo_damage;

                should_post_start = true;
                start_req.game_id = (s.game_id == "ats" || s.game_id == "amtrucks") ? "2" : "1";
                start_req.game_mode = MapServerModeToGameMode(s.server_mode);
                start_req.market_type = market;
                start_req.source_city = src_city;
                start_req.destination_city = dst_city;
                start_req.source_company = src_comp;
                start_req.destination_company = dst_comp;
                start_req.cargo_id = cargo_id;
                start_req.cargo_name = cargo;
                start_req.cargo_mass_tons = mass / 1000.0f;
                start_req.planned_distance_km = planned_km;
                start_req.vehicle_brand = s.truck_brand;
                start_req.vehicle_model = s.truck_name;
                start_req.vehicle_in_game_id = s.truck_in_game_id;
                start_req.fleet_number = matched_fleet_number;
            }
        });

        if (should_post_start && nismara::core::IsCompanionAppRunning()) {
            nismara::core::ApiClient::Instance().PostJobStartAsync(start_req);
        }

        LogToGame(SCS_LOG_TYPE_message, "[NismaraTracker] Active job loaded: %s (%s -> %s)",
                  cargo.c_str(), src_city.c_str(), dst_city.c_str());
    }
}

SCSAPI_VOID OnTelemetryGameplayEvent(
    const scs_event_t /*event*/,
    const void* const event_info,
    const scs_context_t /*context*/) {
    if (!event_info) {
        return;
    }

    const auto* info = static_cast<const scs_telemetry_gameplay_event_t*>(event_info);
    if (!info->id) {
        return;
    }

    if (std::strcmp(info->id, SCS_TELEMETRY_GAMEPLAY_EVENT_player_fined) == 0) {
        const auto* off_attr = FindNamedAttr(
            info->attributes,
            SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_fine_offence,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_string);
        const std::string offence = (off_attr && off_attr->value.value_string.value)
                                        ? off_attr->value.value_string.value
                                        : "police_fine";
        const int64_t fine_amt = GetS64Attribute(
            info->attributes,
            SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_fine_amount,
            0);

        if (fine_amt > 0) {
            nismara::core::TelemetryState::Instance().Write([&](nismara::core::TelemetrySnapshot& s) {
                if (s.job_active) {
                    s.job_fines.emplace_back(offence, fine_amt);
                }
            });
            LogToGame(SCS_LOG_TYPE_message, "[NismaraTracker] Traffic fine recorded: %s (%lld)", offence.c_str(), fine_amt);
        }
        return;
    }

    if (std::strcmp(info->id, SCS_TELEMETRY_GAMEPLAY_EVENT_job_delivered) == 0 ||
        std::strcmp(info->id, SCS_TELEMETRY_GAMEPLAY_EVENT_car_job_delivered) == 0 ||
        std::strcmp(info->id, "job_finished") == 0) {
        const int64_t event_revenue = GetS64Attribute(
            info->attributes,
            SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_revenue,
            0);
        const auto* dist_attr = FindNamedAttr(
            info->attributes,
            SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_distance_km,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_float);
        const float event_dist_km = dist_attr ? dist_attr->value.value_float.value : 0.0f;

        const auto* cdmg_attr = FindNamedAttr(
            info->attributes,
            SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_cargo_damage,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_float);
        const float event_cargo_dmg = cdmg_attr ? cdmg_attr->value.value_float.value : 0.0f;

        const bool auto_park_used = GetBoolAttribute(
            info->attributes,
            SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_auto_park_used,
            false);

        const auto acc = nismara::core::TelemetryState::Instance().GetDriverAccount();
        std::string matched_fleet_number;
        if (acc.active_fleet_index >= 0 && acc.active_fleet_index < static_cast<int>(acc.fleets.size())) {
            matched_fleet_number = acc.fleets[acc.active_fleet_index].fleet_number;
        }

        nismara::core::JobCompleteRequest comp_req{};

        nismara::core::TelemetryState::Instance().Write([&](nismara::core::TelemetrySnapshot& s) {
            comp_req.job_id = s.active_job_db_id;
            comp_req.game_id = (s.game_id == "ats" || s.game_id == "amtrucks") ? "2" : "1";
            comp_req.game_mode = MapServerModeToGameMode(s.server_mode);

            uint32_t driven_km = 0;
            if (event_dist_km > 0.5f) {
                driven_km = static_cast<uint32_t>(std::round(event_dist_km));
            } else if (s.job_start_odometer_km > 0.0f && s.odometer_km > s.job_start_odometer_km) {
                driven_km = static_cast<uint32_t>(std::round(s.odometer_km - s.job_start_odometer_km));
            } else {
                driven_km = s.planned_distance_km;
            }

            // Cap driven distance to at most +25% of planned_distance_km
            if (s.planned_distance_km > 0) {
                const uint32_t max_allowed_km =
                    std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(s.planned_distance_km) * 1.25f)));
                if (driven_km > max_allowed_km) {
                    driven_km = max_allowed_km;
                }
            }
            comp_req.distance_km = std::max(1u, driven_km);

            const int64_t now_sec = static_cast<int64_t>(std::time(nullptr));
            comp_req.duration_seconds = (s.job_start_timestamp_sec > 0 && now_sec > s.job_start_timestamp_sec)
                                            ? (now_sec - s.job_start_timestamp_sec)
                                            : 60;
            comp_req.fuel_used_liters = s.job_fuel_consumed_liters;
            comp_req.max_speed_kmh = std::max(s.job_max_speed_kmh, s.speed_kmh);

            const float max_veh_wear = std::max({
                s.wear_engine,
                s.wear_transmission,
                s.wear_cabin,
                s.wear_chassis,
                s.wear_wheels,
                s.job_accum_vehicle_damage
            });
            const float max_trl_wear = std::max(s.wear_trailer, s.job_accum_trailer_damage);
            const float max_crg_wear = std::max({s.cargo_damage, event_cargo_dmg, s.job_accum_cargo_damage});

            comp_req.vehicle_damage_pct = static_cast<int32_t>(std::round(max_veh_wear * 100.0f));
            comp_req.trailer_damage_pct = static_cast<int32_t>(std::round(max_trl_wear * 100.0f));
            comp_req.cargo_damage_pct = static_cast<int32_t>(std::round(max_crg_wear * 100.0f));
            comp_req.in_game_revenue = (event_revenue > 0) ? static_cast<uint64_t>(event_revenue) : s.reward_payout;
            comp_req.is_hardcore = !auto_park_used;
            comp_req.hardcore_rating = auto_park_used ? 0.0f : 5.0f;
            comp_req.vehicle_in_game_id = s.truck_in_game_id;
            comp_req.fleet_number = matched_fleet_number;
            comp_req.fines = s.job_fines;

            s.job_active = false;
            s.last_job_status = "Contract Completed (job_finished)";
        });

        if (nismara::core::IsCompanionAppRunning()) {
            nismara::core::ApiClient::Instance().PostJobCompleteAsync(comp_req);
            LogToGame(SCS_LOG_TYPE_message, "[NismaraTracker] Gameplay event: Job Delivered -> Submitting to server.");
        }
    } else if (std::strcmp(info->id, SCS_TELEMETRY_GAMEPLAY_EVENT_job_cancelled) == 0 ||
               std::strcmp(info->id, SCS_TELEMETRY_GAMEPLAY_EVENT_car_job_cancelled) == 0 ||
               std::strcmp(info->id, "job_cancelled") == 0) {
        nismara::core::JobCancelRequest cancel_req{};
        nismara::core::TelemetryState::Instance().Write([&](nismara::core::TelemetrySnapshot& s) {
            cancel_req.job_id = s.active_job_db_id;
            cancel_req.game_id = (s.game_id == "ats" || s.game_id == "amtrucks") ? "2" : "1";
            s.job_active = false;
            s.last_job_status = "Contract Aborted (job_cancelled)";
        });
        if (nismara::core::IsCompanionAppRunning()) {
            nismara::core::ApiClient::Instance().PostJobCancelAsync(cancel_req);
            LogToGame(SCS_LOG_TYPE_message, "[NismaraTracker] Gameplay event: Job Cancelled -> Submitting to server.");
        }
    } else if (std::strcmp(info->id, "job_started") == 0) {
        nismara::core::TelemetryState::Instance().Write([](nismara::core::TelemetrySnapshot& s) {
            s.job_active = true;
            s.last_job_status = "Active Contract (job_started)";
        });
    }
}

} // namespace

// ============================================================================
// Exported SCS SDK Entry Points
// ============================================================================

SCSSDK_HEADER

SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params) {
    if (version != SCS_TELEMETRY_VERSION_1_01 && version != SCS_TELEMETRY_VERSION_1_00) {
        return SCS_RESULT_unsupported;
    }
    if (!params) {
        return SCS_RESULT_invalid_parameter;
    }

    const auto* version_params = static_cast<const scs_telemetry_init_params_v101_t*>(params);
    g_game_log = version_params->common.log;

    LogToGame(SCS_LOG_TYPE_message, "[NismaraTracker] SCS Telemetry v1.01 initialized successfully.");

    // Load local configuration (nismara_config.dat)
    nismara::core::ConfigManager::Instance().Load();

    // Ensure DX11 overlay hook is active
    nismara::hooks::Dx11Hook::Instance().StartAsync();

    // Auto-sync account only if Nismara Companion is actively running in background
    if (nismara::core::IsCompanionAppRunning() &&
        !nismara::core::ConfigManager::Instance().GetConfig().driver_token.empty()) {
        nismara::core::ApiClient::Instance().SyncAccountAsync();
    }

    // Store game identity & engine version
    const std::string game_id = version_params->common.game_id ? version_params->common.game_id : "unknown";
    const std::string game_name = version_params->common.game_name ? version_params->common.game_name : "SCS Game";
    const uint32_t major_ver = SCS_GET_MAJOR_VERSION(version_params->common.game_version);
    const uint32_t minor_ver = SCS_GET_MINOR_VERSION(version_params->common.game_version);

    nismara::core::TelemetryState::Instance().Write([&](nismara::core::TelemetrySnapshot& s) {
        s.game_id = game_id;
        s.game_name = game_name;
        s.game_version_major = major_ver;
        s.game_version_minor = minor_ver;
        s.sdk_active = true;
        s.is_paused = true;
    });

    // Start background Server & Multiplayer Detector (Singleplayer / Convoy / TruckersMP / HaulMP)
    nismara::core::ServerDetector::Instance().Initialize();

    // Register core lifecycle & configuration events
    const bool events_ok =
        (version_params->register_for_event(SCS_TELEMETRY_EVENT_paused, OnTelemetryPause, nullptr) == SCS_RESULT_ok) &&
        (version_params->register_for_event(SCS_TELEMETRY_EVENT_started, OnTelemetryPause, nullptr) == SCS_RESULT_ok) &&
        (version_params->register_for_event(SCS_TELEMETRY_EVENT_configuration, OnTelemetryConfiguration, nullptr) == SCS_RESULT_ok);

    if (!events_ok) {
        LogToGame(SCS_LOG_TYPE_error, "[NismaraTracker] Failed to register core telemetry events.");
        g_game_log = nullptr;
        return SCS_RESULT_generic_error;
    }

    // Gameplay events require Telemetry SDK v1.01+
    if (version >= SCS_TELEMETRY_VERSION_1_01) {
        version_params->register_for_event(SCS_TELEMETRY_EVENT_gameplay, OnTelemetryGameplayEvent, nullptr);
    }

    // Register periodic channels for truck mechanical metrics, world position, and wear
    const scs_u32_t flags = SCS_TELEMETRY_CHANNEL_FLAG_no_value;

    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelSpeed, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelRpm, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear, SCS_U32_NIL, SCS_VALUE_TYPE_s32, flags, OnChannelEngineGear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_displayed_gear, SCS_U32_NIL, SCS_VALUE_TYPE_s32, flags, OnChannelDisplayedGear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_engine_enabled, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelEngineEnabled, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_electric_enabled, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelElectricEnabled, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_fuel, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelFuelAmount, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_fuel_range, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelFuelRange, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_odometer, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelOdometer, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelParkingBrake, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_cruise_control, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelCruiseControl, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_motor_brake, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelMotorBrake, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_retarder_level, SCS_U32_NIL, SCS_VALUE_TYPE_u32, flags, OnChannelRetarder, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_high_beam, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelHighBeam, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_hazard_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelHazard, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_differential_lock, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelDiffLock, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_fuel_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelFuelWarning, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_fuel_average_consumption, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelFuelAvgConsumption, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_navigation_speed_limit, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelSpeedLimit, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_oil_temperature, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelOilTemp, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_water_temperature, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWaterTemp, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelBrakeAirPressure, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_battery_voltage, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelBatteryVoltage, nullptr);

    // World space position & orientation (dplacement)
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_world_placement, SCS_U32_NIL, SCS_VALUE_TYPE_dplacement, flags, OnChannelWorldPlacement, nullptr);

    // Structural health / wear channels
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wear_cabin, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wear_chassis, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wear_wheels, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wear_engine, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wear_transmission, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);

    version_params->register_for_channel(SCS_TELEMETRY_TRAILER_CHANNEL_connected, SCS_U32_NIL, SCS_VALUE_TYPE_bool, flags, OnChannelTrailerConnected, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRAILER_CHANNEL_wear_chassis, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRAILER_CHANNEL_wear_body, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRAILER_CHANNEL_wear_wheels, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRAILER_CHANNEL_cargo_damage, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_JOB_CHANNEL_cargo_damage, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelWear, nullptr);

    // Simulation time & navigation channels
    version_params->register_for_channel(SCS_TELEMETRY_CHANNEL_game_time, SCS_U32_NIL, SCS_VALUE_TYPE_u32, flags, OnChannelGameTime, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_CHANNEL_local_scale, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelLocalScale, nullptr);
    version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance, SCS_U32_NIL, SCS_VALUE_TYPE_float, flags, OnChannelNavigationDistance, nullptr);

    LogToGame(SCS_LOG_TYPE_message, "[NismaraTracker] NISMARA OS v1.0 Telemetry initialized for '%s' (SDK v%u.%u). Press F11 to toggle tablet.",
              game_id.c_str(), major_ver, minor_ver);

    return SCS_RESULT_ok;
}

SCSAPI_VOID scs_telemetry_shutdown(void) {
    LogToGame(SCS_LOG_TYPE_message, "[NismaraTracker] Shutting down telemetry session.");
    nismara::core::ServerDetector::Instance().Shutdown();
    g_game_log = nullptr;
    nismara::core::TelemetryState::Instance().Reset();
}

SCSSDK_FOOTER

