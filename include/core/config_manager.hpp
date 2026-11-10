#pragma once

#include <filesystem>
#include <mutex>
#include <string>

namespace nismara::core {

inline constexpr const char* DEFAULT_PRODUCTION_API_URL = "http://103.150.101.154:3100";

struct PluginConfig {
    std::string driver_token{};
    std::string api_base_url{DEFAULT_PRODUCTION_API_URL};
    std::string steam_id{}; // Prepared for future automatic SteamID64 verification
    int toggle_hotkey_vk{0x7A}; // Default: VK_F11 (0x7A)
    bool overlay_start_visible{false};

    // iPhone 16 OS Customization
    std::string lockscreen_wallpaper{"preset:ultramarine"};
    std::string homescreen_wallpaper{"preset:midnight"};
    int phone_frame_color{0}; // 0: Ultramarine (SVG Default), 1: Black Titanium, 2: Natural Titanium, 3: Teal, 4: Pink
    bool auto_lock_on_close{true}; // Always start at Lock Screen when opened with F11

    // Draggable Phone Position on Screen (-1.0f indicates default alignment)
    float phone_pos_x{-1.0f};
    float phone_pos_y{-1.0f};
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
     * @brief Thread-safe update of saved phone overlay position.
     */
    void UpdatePhonePosition(float x, float y);

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
     * @brief Returns the resolved absolute path of nismara_config.dat
     *        (inside Documents\<Game>\nismara_config.dat).
     */
    std::filesystem::path GetConfigFilePath() const;

    /**
     * @brief Returns the resolved directory for custom wallpaper images
     *        (inside Documents\<Game>\wallpapers).
     */
    std::filesystem::path GetWallpapersDirectory() const;

    /**
     * @brief Returns the resolved game user data directory
     *        (e.g., Documents\Euro Truck Simulator 2 or -homedir equivalent),
     *        where game.log.txt, profiles/, save/.../game.sii, and nismara_config.dat reside.
     */
    std::filesystem::path GetGameUserDataDirectory() const;

    /**
     * @brief Returns the cross-platform heartbeat lockfile path used by Companion Gatekeeper
     *        (Documents\<Game>\nismara_companion_alive.lock).
     */
    std::filesystem::path GetHeartbeatFilePath() const;

    /**
     * @brief Resolves the directory where Ultralight runtime DLLs and ICU assets reside
     *        (%LOCALAPPDATA%\Nismara Companion\runtime).
     */
    std::filesystem::path GetCompanionRuntimeDirectory() const;

    /**
     * @brief Resolves the directory where compiled React + Tailwind web UI files reside
     *        (%LOCALAPPDATA%\Nismara Companion\web-ui).
     */
    std::filesystem::path GetCompanionWebUIDirectory() const;

private:
    ConfigManager();
    ~ConfigManager() = default;

    void ResolvePluginDirectory();

    mutable std::mutex mutex_;
    PluginConfig config_{};
    std::filesystem::path game_user_dir_{};
    std::filesystem::path config_path_{};
    std::filesystem::path wallpapers_dir_{};
    std::filesystem::path heartbeat_path_{};
};

} // namespace nismara::core
