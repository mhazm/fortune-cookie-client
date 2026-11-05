#include "core/profile_validator.hpp"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace nismara::core {

namespace {

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string ReadFileShared(const std::filesystem::path& path, size_t max_bytes = 2097152) { // up to 2MB
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

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
        CloseHandle(hFile);
        return {};
    }

    size_t bytesToRead = static_cast<size_t>(fileSize.QuadPart);
    if (bytesToRead > max_bytes) {
        bytesToRead = max_bytes;
        // Seek to the last max_bytes of the file to capture recent profile load events
        LARGE_INTEGER seekPos;
        seekPos.QuadPart = fileSize.QuadPart - static_cast<LONGLONG>(max_bytes);
        SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);
    }

    std::string buffer(bytesToRead, '\0');
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(bytesToRead), &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return {};
    }
    CloseHandle(hFile);

    buffer.resize(bytesRead);
    return buffer;
}

void ParseCvarLine(const std::string& line, HardcoreProfileStatus& status) {
    size_t usetPos = line.find("uset ");
    if (usetPos == std::string::npos) return;

    size_t nameStart = usetPos + 5;
    size_t nameEnd = line.find(' ', nameStart);
    if (nameEnd == std::string::npos) return;

    std::string varName = line.substr(nameStart, nameEnd - nameStart);

    size_t quoteStart = line.find('"', nameEnd);
    if (quoteStart == std::string::npos) return;
    size_t quoteEnd = line.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return;

    std::string varValue = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

    try {
        if (varName == "g_traffic") {
            status.traffic_val = std::stof(varValue);
            status.traffic_ok = (status.traffic_val >= 1.0f);
        } else if (varName == "g_fatigue") {
            status.fatigue_val = std::stoi(varValue);
            status.fatigue_ok = (status.fatigue_val == 1);
        } else if (varName == "g_police") {
            status.police_val = std::stoi(varValue);
            status.police_ok = (status.police_val == 1);
        } else if (varName == "g_parking_difficulty") {
            status.parking_val = std::stoi(varValue);
            if (status.parking_val == 2) {
                status.parking_ok = true; // 2 = Skilled Parking
            }
        } else if (varName == "g_simple_parking_doubles") {
            if (std::stoi(varValue) == 0) {
                status.parking_ok = true;
            }
        } else if (varName == "g_road_events") {
            status.road_events_val = std::stof(varValue);
            status.road_events_ok = (status.road_events_val >= 0.10f);
        } else if (varName == "g_bad_weather_factor" || varName == "g_rain_prob") {
            status.rain_prob_val = std::stof(varValue);
            status.rain_prob_ok = (status.rain_prob_val >= 0.10f);
        } else if (varName == "g_trailer_advanced_coupling") {
            status.advanced_coupling_val = std::stoi(varValue);
            status.advanced_coupling_ok = (status.advanced_coupling_val == 1);
        } else if (varName == "g_fuel_simulation") {
            status.fuel_simulation_val = std::stoi(varValue);
            status.fuel_simulation_ok = (status.fuel_simulation_val == 1);
        } else if (varName == "g_hardcore_simulation") {
            status.air_brake_val = std::stoi(varValue);
            status.air_brake_ok = (status.air_brake_val == 1);
        }
    } catch (...) {
        // Ignore parsing errors for non-numeric/custom cvars
    }
}

} // namespace

HardcoreProfileStatus ProfileValidator::ScanProfileSettings(const std::filesystem::path& game_dir) {
    HardcoreProfileStatus status;
    if (game_dir.empty()) {
        return status;
    }

    try {
        // 1. First parse master config.cfg for default fallback values (e.g. g_traffic)
        std::filesystem::path configPath = game_dir / "config.cfg";
        std::string configContent = ReadFileShared(configPath, 131072);
        if (!configContent.empty()) {
            std::istringstream stream(configContent);
            std::string line;
            while (std::getline(stream, line)) {
                ParseCvarLine(line, status);
            }
        }

        // 2. Parse game.log.txt which contains the live profile cvars and loaded mod packages
        std::filesystem::path logPath = game_dir / "game.log.txt";
        std::string logContent = ReadFileShared(logPath, 2097152); // scan last 2MB of log
        if (!logContent.empty()) {
            status.scanned = true;
            std::istringstream stream(logContent);
            std::string line;

            static const std::vector<std::string> forbiddenKeywords = {
                "no damage", "no_damage", "nodamage", "zero damage", "0% damage",
                "unlimited fuel", "infinite fuel", "infinit fuel", "free fuel",
                "no police", "no fines", "nofines",
                "disable fatigue", "no fatigue",
                "speed limit remover", "no speed limit",
                "unlimited money", "easy money", "economy mod"
            };

            while (std::getline(stream, line)) {
                // Check cvars in game.log.txt (profile overrides)
                if (line.find("uset ") != std::string::npos) {
                    ParseCvarLine(line, status);
                }

                // Check for forbidden mods in mounting lines
                if (line.find("Mounting package") != std::string::npos ||
                    line.find("[zipfs]") != std::string::npos ||
                    line.find("[hashfs]") != std::string::npos) {
                    
                    std::string lowerLine = ToLowerAscii(line);
                    for (const auto& kw : forbiddenKeywords) {
                        if (lowerLine.find(kw) != std::string::npos) {
                            status.has_forbidden_mods = true;
                            // Extract package name cleanly
                            size_t colon = line.rfind(':');
                            std::string modName = (colon != std::string::npos) ? line.substr(0, colon) : line;
                            status.forbidden_mods.push_back(modName);
                            break;
                        }
                    }
                }
            }
        }
    } catch (...) {}

    return status;
}

} // namespace nismara::core

