#include "core/config_manager.hpp"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
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
        return DEFAULT_PRODUCTION_API_URL;
    }
    return cleaned;
}

std::wstring DetectGameFolderName() {
    wchar_t exe_path[MAX_PATH] = {0};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0) {
        std::wstring lower(exe_path);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        if (lower.find(L"amtrucks") != std::wstring::npos ||
            lower.find(L"american truck simulator") != std::wstring::npos) {
            return L"American Truck Simulator";
        }
    }
    return L"Euro Truck Simulator 2";
}

std::filesystem::path ExtractHomeDirFromCommandLine(const std::wstring& game_folder_name) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return {};
    }

    std::filesystem::path result{};
    for (int i = 0; i < argc - 1; ++i) {
        std::wstring arg(argv[i]);
        std::transform(arg.begin(), arg.end(), arg.begin(), ::towlower);
        if (arg == L"-homedir" && argv[i + 1] != nullptr && argv[i + 1][0] != L'\0') {
            std::filesystem::path candidate(argv[i + 1]);
            std::error_code ec;
            if (std::filesystem::exists(candidate / game_folder_name, ec)) {
                result = candidate / game_folder_name;
            } else {
                result = candidate / game_folder_name;
            }
            break;
        }
    }

    LocalFree(argv);
    return result;
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
    const std::wstring game_folder_name = DetectGameFolderName();

    // 1. Resolve plugin DLL directory (for legacy migration cleanup)
    wchar_t module_path[MAX_PATH] = {0};
    HMODULE hMod = nullptr;
    std::filesystem::path dll_dir{};
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ConfigManager::Instance),
            &hMod) &&
        hMod != nullptr) {
        if (GetModuleFileNameW(hMod, module_path, MAX_PATH) > 0) {
            dll_dir = std::filesystem::path(module_path).parent_path();
        }
    }

    // 2. Resolve Game User Data Directory (Documents\Euro Truck Simulator 2 or -homedir equivalent)
    std::filesystem::path user_data_dir = ExtractHomeDirFromCommandLine(game_folder_name);

    if (user_data_dir.empty()) {
        wchar_t docs_path[MAX_PATH] = {0};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs_path)) &&
            docs_path[0] != L'\0') {
            user_data_dir = std::filesystem::path(docs_path) / game_folder_name;
        }
    }

    if (user_data_dir.empty()) {
        // Fallback via USERPROFILE or HOME environment variables (Wine/Linux/macOS)
        wchar_t env_buf[MAX_PATH] = {0};
        if (GetEnvironmentVariableW(L"USERPROFILE", env_buf, MAX_PATH) > 0) {
            user_data_dir = std::filesystem::path(env_buf) / L"Documents" / game_folder_name;
        } else if (GetEnvironmentVariableW(L"HOME", env_buf, MAX_PATH) > 0) {
            user_data_dir = std::filesystem::path(env_buf) / L".local" / L"share" / game_folder_name;
        } else if (!dll_dir.empty()) {
            user_data_dir = dll_dir;
        } else {
            user_data_dir = std::filesystem::current_path();
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(user_data_dir, ec);

    game_user_dir_ = user_data_dir;
    config_path_ = game_user_dir_ / L"nismara_config.dat";
    wallpapers_dir_ = game_user_dir_ / L"wallpapers";
    heartbeat_path_ = game_user_dir_ / L"nismara_companion_alive.lock";

    std::filesystem::create_directories(wallpapers_dir_, ec);

    // 3. Auto-migrate legacy plugins/nismara_config.dat & plugins/wallpapers/ into Documents\<Game>\
    //    so the plugins/ folder remains 100% clean (containing ONLY NismaraTracker.dll).
    if (!dll_dir.empty() && dll_dir != game_user_dir_) {
        const auto legacy_cfg = dll_dir / L"nismara_config.dat";
        if (std::filesystem::exists(legacy_cfg, ec)) {
            if (!std::filesystem::exists(config_path_, ec)) {
                std::filesystem::copy_file(
                    legacy_cfg,
                    config_path_,
                    std::filesystem::copy_options::overwrite_existing,
                    ec);
            }
            std::filesystem::remove(legacy_cfg, ec);
        }

        const auto legacy_wallpapers = dll_dir / L"wallpapers";
        if (std::filesystem::exists(legacy_wallpapers, ec) &&
            std::filesystem::is_directory(legacy_wallpapers, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(legacy_wallpapers, ec)) {
                if (entry.is_regular_file(ec)) {
                    const auto dest_file = wallpapers_dir_ / entry.path().filename();
                    if (!std::filesystem::exists(dest_file, ec)) {
                        std::filesystem::copy_file(entry.path(), dest_file, ec);
                    }
                }
            }
            std::filesystem::remove_all(legacy_wallpapers, ec);
        }
    }
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
        } else if (key == "phone_pos_x") {
            try {
                config_.phone_pos_x = std::stof(value);
            } catch (...) {
                config_.phone_pos_x = -1.0f;
            }
        } else if (key == "phone_pos_y") {
            try {
                config_.phone_pos_y = std::stof(value);
            } catch (...) {
                config_.phone_pos_y = -1.0f;
            }
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

    file << "# Nismara Companion Configuration\n";
    file << "driver_token=" << config_.driver_token << "\n";
    file << "api_base_url=" << config_.api_base_url << "\n";
    file << "steam_id=" << config_.steam_id << "\n";
    file << "toggle_hotkey_vk=" << config_.toggle_hotkey_vk << "\n";
    file << "overlay_start_visible=" << (config_.overlay_start_visible ? 1 : 0) << "\n";
    file << "lockscreen_wallpaper=" << config_.lockscreen_wallpaper << "\n";
    file << "homescreen_wallpaper=" << config_.homescreen_wallpaper << "\n";
    file << "phone_frame_color=" << config_.phone_frame_color << "\n";
    file << "auto_lock_on_close=" << (config_.auto_lock_on_close ? 1 : 0) << "\n";
    file << "phone_pos_x=" << config_.phone_pos_x << "\n";
    file << "phone_pos_y=" << config_.phone_pos_y << "\n";
    file.flush();

    return file.good();
}

void ConfigManager::UpdatePhonePosition(float x, float y) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.phone_pos_x = x;
    config_.phone_pos_y = y;
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

std::filesystem::path ConfigManager::GetGameUserDataDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return game_user_dir_;
}

std::filesystem::path ConfigManager::GetHeartbeatFilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heartbeat_path_;
}

std::filesystem::path ConfigManager::GetCompanionRuntimeDirectory() const {
    wchar_t local_app_data[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH) > 0) {
        const std::filesystem::path base = std::filesystem::path(local_app_data) / L"Nismara Companion";
        std::error_code ec;
        const auto res_runtime = base / L"resources" / L"runtime";
        if (std::filesystem::exists(res_runtime / L"Ultralight.dll", ec)) {
            return res_runtime;
        }
        const auto direct_runtime = base / L"runtime";
        if (std::filesystem::exists(direct_runtime / L"Ultralight.dll", ec)) {
            return direct_runtime;
        }
        return res_runtime;
    }
    return {};
}

std::filesystem::path ConfigManager::GetCompanionWebUIDirectory() const {
    wchar_t local_app_data[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH) > 0) {
        const std::filesystem::path base = std::filesystem::path(local_app_data) / L"Nismara Companion";
        std::error_code ec;
        const auto res_webui = base / L"resources" / L"web-ui";
        if (std::filesystem::exists(res_webui / L"index.html", ec)) {
            return res_webui;
        }
        const auto direct_webui = base / L"web-ui";
        if (std::filesystem::exists(direct_webui / L"index.html", ec)) {
            return direct_webui;
        }
        return res_webui;
    }
    return {};
}

} // namespace nismara::core
