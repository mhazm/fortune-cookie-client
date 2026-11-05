#pragma once

#include <windows.h>
#include <chrono>
#include <filesystem>
#include "core/config_manager.hpp"

namespace nismara::core {

/**
 * @brief Checks if Nismara Companion Desktop App is currently running in the background.
 *        1. Fast Path (Native Windows): Checks Win32 Named Mutex (Global\NismaraCompanionRunning
 *           or Local\NismaraCompanionRunning).
 *        2. Cross-Platform / Linux Proton / macOS Path: Checks the heartbeat file
 *           (Documents\<Game>\nismara_companion_alive.lock) updated every few seconds
 *           by Nismara Companion, throttled to avoid per-frame disk I/O.
 */
inline bool IsCompanionAppRunning() {
    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Global\\NismaraCompanionRunning");
    if (hMutex == nullptr) {
        hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\NismaraCompanionRunning");
    }
    if (hMutex != nullptr) {
        CloseHandle(hMutex);
        return true;
    }

    // Fallback for Linux Steam Proton/Wine & cross-platform environments
    static ULONGLONG last_check_tick = 0;
    static bool cached_heartbeat_alive = false;

    const ULONGLONG now_tick = GetTickCount64();
    if (now_tick - last_check_tick >= 1500 || last_check_tick == 0) {
        last_check_tick = now_tick;
        std::error_code ec;
        const auto hb_path = ConfigManager::Instance().GetHeartbeatFilePath();
        if (!hb_path.empty() && std::filesystem::exists(hb_path, ec)) {
            const auto ftime = std::filesystem::last_write_time(hb_path, ec);
            if (!ec) {
                const auto now_ftime = std::filesystem::file_time_type::clock::now();
                const auto age_sec = std::chrono::duration_cast<std::chrono::seconds>(now_ftime - ftime).count();
                cached_heartbeat_alive = (age_sec >= -5 && age_sec <= 15);
            } else {
                cached_heartbeat_alive = false;
            }
        } else {
            cached_heartbeat_alive = false;
        }
    }

    return cached_heartbeat_alive;
}

} // namespace nismara::core

