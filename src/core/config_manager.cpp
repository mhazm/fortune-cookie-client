#include "core/config_manager.hpp"

#include <windows.h>
#include <fstream>
#include <sstream>

namespace nismara::core {

namespace {
std::string Trim(const std::string& str) {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string NormalizeBaseUrl(const std::string& url) {
    std::string cleaned = Trim(url);
    while (!cleaned.empty() && cleaned.back() == '/') {
        cleaned.pop_back();
    }
    if (cleaned.empty()) {
        return "http://localhost:3100";
    }
    return cleaned;
}
} // namespace

ConfigManager& ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    ResolvePluginDirectory();
}

void ConfigManager::ResolvePluginDirectory() {
    wchar_t module_path[MAX_PATH] = {0};
    HMODULE hMod = nullptr;

    std::filesystem::path base_dir = std::filesystem::current_path();
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ConfigManager::Instance),
            &hMod) &&
        hMod != nullptr) {
        if (GetModuleFileNameW(hMod, module_path, MAX_PATH) > 0) {
            base_dir = std::filesystem::path(module_path).parent_path();
        }
    }

    config_path_ = base_dir / L"nismara_config.dat";
    wallpapers_dir_ = base_dir / L"wallpapers";

    std::error_code ec;
    std::filesystem::create_directories(wallpapers_dir_, ec);
}

bool ConfigManager::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_path_.empty()) {
        ResolvePluginDirectory();
    }

    std::ifstream file(config_path_, std::ios::in);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        const auto sep = line.find('=');
        if (sep == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, sep));
        const std::string value = Trim(line.substr(sep + 1));

        if (key == "driver_token") {
            config_.driver_token = value;
        } else if (key == "api_base_url") {
            config_.api_base_url = NormalizeBaseUrl(value);
        } else if (key == "steam_id") {
            config_.steam_id = value;
        } else if (key == "toggle_hotkey_vk") {
            try {
                config_.toggle_hotkey_vk = std::stoi(value);
            } catch (...) {
                config_.toggle_hotkey_vk = VK_F11;
            }
        } else if (key == "overlay_start_visible") {
            config_.overlay_start_visible = (value == "1" || value == "true");
        } else if (key == "lockscreen_wallpaper" && !value.empty()) {
            config_.lockscreen_wallpaper = value;
        } else if (key == "homescreen_wallpaper" && !value.empty()) {
            config_.homescreen_wallpaper = value;
        } else if (key == "phone_frame_color") {
            try {
                config_.phone_frame_color = std::clamp(std::stoi(value), 0, 4);
            } catch (...) {
                config_.phone_frame_color = 0;
            }
        } else if (key == "auto_lock_on_close") {
            config_.auto_lock_on_close = (value == "1" || value == "true");
        }
    }

    return true;
}

bool ConfigManager::Save() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_path_.empty()) {
        return false;
    }

    std::ofstream file(config_path_, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << "# Nismara OS iPhone 16 Simulation Unit Configuration\n";
    file << "driver_token=" << config_.driver_token << "\n";
    file << "api_base_url=" << config_.api_base_url << "\n";
    file << "steam_id=" << config_.steam_id << "\n";
    file << "toggle_hotkey_vk=" << config_.toggle_hotkey_vk << "\n";
    file << "overlay_start_visible=" << (config_.overlay_start_visible ? 1 : 0) << "\n";
    file << "lockscreen_wallpaper=" << config_.lockscreen_wallpaper << "\n";
    file << "homescreen_wallpaper=" << config_.homescreen_wallpaper << "\n";
    file << "phone_frame_color=" << config_.phone_frame_color << "\n";
    file << "auto_lock_on_close=" << (config_.auto_lock_on_close ? 1 : 0) << "\n";
    file.flush();

    return file.good();
}

PluginConfig ConfigManager::GetConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ConfigManager::SetDriverToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.driver_token = Trim(token);
}

void ConfigManager::SetApiBaseUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.api_base_url = NormalizeBaseUrl(url);
}

void ConfigManager::SetSteamId(const std::string& steam_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.steam_id = Trim(steam_id);
}

void ConfigManager::SetLockscreenWallpaper(const std::string& spec) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.lockscreen_wallpaper = Trim(spec);
}

void ConfigManager::SetHomescreenWallpaper(const std::string& spec) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.homescreen_wallpaper = Trim(spec);
}

void ConfigManager::SetPhoneFrameColor(int color_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.phone_frame_color = std::clamp(color_index, 0, 4);
}

void ConfigManager::SetAutoLockOnClose(bool auto_lock) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.auto_lock_on_close = auto_lock;
}

std::filesystem::path ConfigManager::GetConfigFilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_path_;
}

std::filesystem::path ConfigManager::GetWallpapersDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wallpapers_dir_;
}

} // namespace nismara::core
