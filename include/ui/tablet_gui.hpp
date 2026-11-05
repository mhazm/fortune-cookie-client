#pragma once

#include "core/telemetry_state.hpp"
#include <atomic>
#include <string>

struct ImDrawList;
struct ImFont;
struct ImVec2;

namespace nismara::ui {

enum class PhoneScreenState {
    LockScreen,
    HomeScreen,
    ActiveApp
};

enum class PhoneAppId {
    None,
    Telemetry,
    Dispatch,
    NismaraId,
    Garage,
    PenaltyPoints,
    Wallpapers,
    Settings
};

/**
 * @brief Renders the in-game Dear ImGui iPhone 16 smartphone interface
 *        modeled after IPhone_16_Vector.svg ("NISMARA iOS 18 SIMULATION UNIT").
 */
class TabletGui {
public:
    static TabletGui& Instance();

    TabletGui(const TabletGui&) = delete;
    TabletGui& operator=(const TabletGui&) = delete;

    /**
     * @brief Applies the base ImGui styling and loads Windows TrueType iOS-style fonts.
     */
    void InitializeStyle();

    /**
     * @brief Renders the iPhone 16 window during IDXGISwapChain::Present when visible.
     */
    void Render();

    /**
     * @brief Toggles the visibility of the phone overlay (resets to Lock Screen if configured).
     */
    void ToggleVisible();

    /**
     * @brief Sets the visibility of the phone overlay.
     */
    void SetVisible(bool visible) noexcept;

    /**
     * @brief Returns true if the phone overlay is currently visible.
     */
    bool IsVisible() const noexcept { return visible_.load(); }

    /**
     * @brief Locks the phone back to the Lock Screen.
     */
    void LockPhone() noexcept;

    /**
     * @brief Unlocks the phone to the Home Screen.
     */
    void UnlockToHome() noexcept;

    /**
     * @brief Opens a specific app on the phone.
     */
    void OpenApp(PhoneAppId app) noexcept;

private:
    TabletGui();
    ~TabletGui() = default;

    void LoadTrueTypeFonts();

    // Hardware Frame & Wallpaper Rendering
    void RenderPhoneBackplate(
        ImDrawList* draw_list,
        const ImVec2& win_pos,
        const ImVec2& win_size,
        int frame_color_idx);

    void RenderPhoneBezelOverlay(
        ImDrawList* draw_list,
        const ImVec2& win_pos,
        const ImVec2& win_size,
        int frame_color_idx);

    void RenderScreenWallpaper(
        ImDrawList* draw_list,
        const ImVec2& screen_min,
        const ImVec2& screen_max,
        bool is_lockscreen);

    void RenderStatusBarAndDynamicIsland(
        ImDrawList* draw_list,
        const ImVec2& screen_min,
        const ImVec2& screen_max,
        const nismara::core::TelemetrySnapshot& snap,
        const nismara::core::DriverAccountProfile& account);

    void RenderHomeIndicatorBar(
        ImDrawList* draw_list,
        const ImVec2& screen_min,
        const ImVec2& screen_max,
        bool is_lockscreen);

    void RenderActiveHoverPill(ImDrawList* draw_list);

    // Screen States
    void RenderLockScreen(
        ImDrawList* draw_list,
        const ImVec2& screen_min,
        const ImVec2& screen_max,
        const nismara::core::TelemetrySnapshot& snap,
        const nismara::core::DriverAccountProfile& account);

    void RenderHomeScreen(
        ImDrawList* draw_list,
        const ImVec2& screen_min,
        const ImVec2& screen_max,
        const nismara::core::TelemetrySnapshot& snap,
        const nismara::core::DriverAccountProfile& account);

    void RenderActiveAppView(
        ImDrawList* draw_list,
        const ImVec2& screen_min,
        const ImVec2& screen_max,
        const nismara::core::TelemetrySnapshot& snap,
        const nismara::core::DriverAccountProfile& account);

    // Individual App Views
    void RenderAppTelemetry(const nismara::core::TelemetrySnapshot& snap);
    void RenderAppDispatch(const nismara::core::TelemetrySnapshot& snap);
    void RenderAppNismaraId(const nismara::core::DriverAccountProfile& account);
    void RenderAppGarage(
        const nismara::core::TelemetrySnapshot& snap,
        const nismara::core::DriverAccountProfile& account);
    void RenderAppPenaltyPoints(
        const nismara::core::TelemetrySnapshot& snap,
        const nismara::core::DriverAccountProfile& account);
    void RenderAppWallpapers(const nismara::core::DriverAccountProfile& account);
    void RenderAppSettings(
        const nismara::core::TelemetrySnapshot& snap,
        const nismara::core::DriverAccountProfile& account);
    void RenderFleetDetailInspector(
        const nismara::core::DriverFleetVehicle& fl,
        const nismara::core::TelemetrySnapshot& snap,
        bool is_active_in_game);

    std::atomic<bool> visible_{false};
    PhoneScreenState screen_state_{PhoneScreenState::LockScreen};
    PhoneAppId active_app_{PhoneAppId::None};
    bool dynamic_island_expanded_{false};
    float unlock_anim_progress_{0.0f};
    bool unlocking_in_progress_{false};

    // Sub-navigation & Drill-Down Navigation Stack
    int garage_tab_{0};          // 0: Overview, 1: Fleet Roster, 2: Facility
    int selected_fleet_idx_{-1}; // -1: Fleet list, >=0: Vehicle Detail Inspector
    int job_tab_{0};             // 0: Route Waybill, 1: Financial Paycheck
    int telemetry_tab_{0};       // 0: Cockpit Cluster, 1: Powertrain & Sensors, 2: Wear Diagnostics
    int penalty_tab_{0};         // 0: Tebus via NC, 1: Tiket Safebox, 2: Aturan Penalti
    int wallpapers_tab_{0};      // 0: Preset iOS 18, 1: Custom Photos, 2: Casing iPhone 16

    // Penalty Points Stepper state
    int32_t penalty_pts_to_pay_{1};
    int32_t penalty_tickets_to_use_{1};

    // TrueType Fonts (Segoe UI Regular, Semibold, Small, Large Number, Lock Screen Clock)
    ImFont* font_regular_{nullptr};
    ImFont* font_semibold_{nullptr};
    ImFont* font_small_{nullptr};
    ImFont* font_large_num_{nullptr};
    ImFont* font_clock_{nullptr};

    // Custom Centered iOS Hover Pill state
    std::string hover_pill_text_{};
    float hover_pill_cx_{0.0f};
    float hover_pill_y_{0.0f};

    // Input Buffers & State
    char token_buffer_[256]{0};
    char api_url_buffer_[256]{0};
    bool config_loaded_{false};
    bool mask_token_{true};
    bool auto_lock_on_close_{true};
    int frame_color_idx_{0};
    std::string lockscreen_wallpaper_spec_{"preset:ultramarine"};
    std::string homescreen_wallpaper_spec_{"preset:midnight"};
    std::string save_status_message_{};
    bool save_status_success_{true};
};

} // namespace nismara::ui
