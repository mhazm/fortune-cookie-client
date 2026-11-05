#pragma once

#include <filesystem>
#include <mutex>
#include <string>

namespace nismara::core {

struct PluginConfig {
    std::string driver_token{};
    std::string api_base_url{"http://localhost:3100"};
    std::string steam_id{}; // Prepared for future automatic SteamID64 verification
    int toggle_hotkey_vk{0x7A}; // Default: VK_F11 (0x7A)
    bool overlay_start_visible{false};

    // iPhone 16 OS Customization
    std::string lockscreen_wallpaper{"preset:ultramarine"};
    std::string homescreen_wallpaper{"preset:midnight"};
    int phone_frame_color{0}; // 0: Ultramarine (SVG Default), 1: Black Titanium, 2: Natural Titanium, 3: Teal, 4: Pink
    bool auto_lock_on_close{true}; // Always start at Lock Screen when opened with F11
};

/**
 * @brief Manages persistent local storage of driver credentials, API endpoint,
 *        and iPhone 16 UI settings in nismara_config.dat inside the plugins directory.
 */
class ConfigManager {
public:
    static ConfigManager& Instance();

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /**
     * @brief Resolves the plugin directory and loads nismara_config.dat if present.
     */
    bool Load();

    /**
     * @brief Persists current configuration to nismara_config.dat.
     */
    bool Save() const;

    /**
     * @brief Thread-safe getter for current configuration.
     */
    PluginConfig GetConfig() const;

    /**
     * @brief Thread-safe setter for driver authentication token.
     */
    void SetDriverToken(const std::string& token);

    /**
     * @brief Thread-safe setter for backend API base URL.
     */
    void SetApiBaseUrl(const std::string& url);

    /**
     * @brief Thread-safe setter for driver SteamID64 (prepared for future verification).
     */
    void SetSteamId(const std::string& steam_id);

    /**
     * @brief Thread-safe setters for iPhone 16 appearance & behavior.
     */
    void SetLockscreenWallpaper(const std::string& spec);
    void SetHomescreenWallpaper(const std::string& spec);
    void SetPhoneFrameColor(int color_index);
    void SetAutoLockOnClose(bool auto_lock);

    /**
     * @brief Returns the resolved absolute path of nismara_config.dat.
     */
    std::filesystem::path GetConfigFilePath() const;

    /**
     * @brief Returns the resolved directory for custom wallpaper images (plugins/wallpapers).
     */
    std::filesystem::path GetWallpapersDirectory() const;

private:
    ConfigManager();
    ~ConfigManager() = default;

    void ResolvePluginDirectory();

    mutable std::mutex mutex_;
    PluginConfig config_{};
    std::filesystem::path config_path_{};
    std::filesystem::path wallpapers_dir_{};
};

} // namespace nismara::core
