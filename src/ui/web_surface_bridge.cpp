#include "ui/web_surface_bridge.hpp"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251 4201 4100)
#endif

#include <Ultralight/Ultralight.h>
#include <Ultralight/JS.h>
#include <AppCore/Platform.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "core/config_manager.hpp"
#include "core/profile_validator.hpp"
#include "core/telemetry_state.hpp"
#include "core/api_client.hpp"
#include "ui/tablet_gui.hpp"

#include "json.hpp"
#include <filesystem>
#include <iostream>

using namespace ultralight;

namespace nismara::ui {

struct WebSurfaceBridge::Impl : public ViewListener, public LoadListener {
    RefPtr<Renderer> renderer;
    RefPtr<View> view;
    std::unique_ptr<js::API> api;

    ID3D11Texture2D* web_texture = nullptr;
    ID3D11ShaderResourceView* web_srv = nullptr;

    bool is_initialized = false;
    bool init_attempted = false;
    bool is_page_loaded = false;
    int last_mouse_x = 175;
    int last_mouse_y = 379;
    std::string web_dir_path;

    virtual ~Impl() {
        ReleaseD3D();
        view = nullptr;
        renderer = nullptr;
    }

    void ReleaseD3D() {
        if (web_srv) {
            web_srv->Release();
            web_srv = nullptr;
        }
        if (web_texture) {
            web_texture->Release();
            web_texture = nullptr;
        }
    }

    // ViewListener callbacks
    void OnAddConsoleMessage([[maybe_unused]] View* caller, const ConsoleMessage& message) override {
        OutputDebugStringA("[Ultralight Console] ");
        OutputDebugStringA(message.message().utf8().data());
        OutputDebugStringA("\n");
    }

    // LoadListener callbacks
    void OnFinishLoading([[maybe_unused]] View* caller, [[maybe_unused]] uint64_t frame_id, [[maybe_unused]] bool is_main_frame, [[maybe_unused]] const String& url) override {
        is_page_loaded = true;
        OutputDebugStringA("[Ultralight] Page finished loading successfully\n");
    }

    void OnFailLoading(
        [[maybe_unused]] View* caller,
        [[maybe_unused]] uint64_t frame_id,
        [[maybe_unused]] bool is_main_frame,
        const String& url,
        const String& description,
        const String& error_domain,
        int error_code) override {
        char buf[512];
        snprintf(
            buf,
            sizeof(buf),
            "[Ultralight Fail] URL: %s, Desc: %s, Domain: %s, Code: %d\n",
            url.utf8().data(),
            description.utf8().data(),
            error_domain.utf8().data(),
            error_code);
        OutputDebugStringA(buf);
    }
};

WebSurfaceBridge& WebSurfaceBridge::Instance() {
    static WebSurfaceBridge instance;
    return instance;
}

WebSurfaceBridge::WebSurfaceBridge()
    : impl_(std::make_unique<Impl>()) {
}

WebSurfaceBridge::~WebSurfaceBridge() {
    Shutdown();
}

bool WebSurfaceBridge::Initialize() {
    if (impl_->is_initialized) {
        return true;
    }
    if (impl_->init_attempted) {
        return false;
    }
    impl_->init_attempted = true;

    try {
        const auto runtime_dir = core::ConfigManager::Instance().GetCompanionRuntimeDirectory();
        std::error_code ec;
        if (!std::filesystem::exists(runtime_dir, ec)) {
            OutputDebugStringA("[WebSurfaceBridge] Companion runtime directory not found\n");
            return false;
        }

        const auto web_dir = core::ConfigManager::Instance().GetCompanionWebUIDirectory();
        if (!std::filesystem::exists(web_dir / "index.html", ec)) {
            OutputDebugStringA("[WebSurfaceBridge] Companion web-ui/index.html not found\n");
            return false;
        }

        // 1. Verify all required Ultralight runtime binaries and ICU data exist
        const auto core_dll = runtime_dir / L"UltralightCore.dll";
        const auto web_dll = runtime_dir / L"WebCore.dll";
        const auto app_dll = runtime_dir / L"AppCore.dll";
        const auto ul_dll = runtime_dir / L"Ultralight.dll";
        const auto icu_dat = runtime_dir / L"icudt67l.dat";

        if (!std::filesystem::exists(core_dll, ec) ||
            !std::filesystem::exists(web_dll, ec) ||
            !std::filesystem::exists(app_dll, ec) ||
            !std::filesystem::exists(ul_dll, ec) ||
            !std::filesystem::exists(icu_dat, ec)) {
            OutputDebugStringA("[WebSurfaceBridge] Missing Ultralight runtime DLLs or icudt67l.dat, safely falling back to native ImGui Phone OS\n");
            return false;
        }

        // 2. Explicitly configure DLL search paths and preload Ultralight runtime DLLs
        SetDllDirectoryW(runtime_dir.c_str());
        AddDllDirectory(runtime_dir.c_str());
        HMODULE h_core = LoadLibraryW(core_dll.c_str());
        HMODULE h_web = LoadLibraryW(web_dll.c_str());
        HMODULE h_app = LoadLibraryW(app_dll.c_str());
        HMODULE h_ul = LoadLibraryW(ul_dll.c_str());

        if (!h_core || !h_web || !h_app || !h_ul) {
            OutputDebugStringA("[WebSurfaceBridge] Failed to load one or more Ultralight runtime DLLs, safely falling back to native ImGui Phone OS\n");
            return false;
        }

        // 2. Ensure resources/ directory inside web-ui has ICU data and certificates
        const auto web_resources = web_dir / "resources";
        if (!std::filesystem::exists(web_resources, ec)) {
            std::filesystem::create_directories(web_resources, ec);
        }
        if (!std::filesystem::exists(web_resources / "icudt67l.dat", ec) &&
            std::filesystem::exists(runtime_dir / "icudt67l.dat", ec)) {
            std::filesystem::copy_file(runtime_dir / "icudt67l.dat", web_resources / "icudt67l.dat", std::filesystem::copy_options::overwrite_existing, ec);
        }
        if (!std::filesystem::exists(web_resources / "cacert.pem", ec) &&
            std::filesystem::exists(runtime_dir / "cacert.pem", ec)) {
            std::filesystem::copy_file(runtime_dir / "cacert.pem", web_resources / "cacert.pem", std::filesystem::copy_options::overwrite_existing, ec);
        }

        std::string web_dir_str = web_dir.string();
        if (!web_dir_str.empty() && web_dir_str.back() != '/' && web_dir_str.back() != '\\') {
            web_dir_str += '/';
        }
        impl_->web_dir_path = web_dir_str;

        // 3. Configure Ultralight Platform & Logger
        const auto log_path = (runtime_dir / "ultralight.log").string();
        Platform::instance().set_logger(GetDefaultLogger(String(log_path.c_str())));

        Config config;
        config.resource_path_prefix = "resources/";
        config.memory_cache_size = 32 * 1024 * 1024; // 32 MB cache (down from default 256 MB)
        config.page_cache_size = 0;
        config.num_renderer_threads = 1; // 1 dedicated renderer thread (prevents spawning 12-16 CPU threads)
        config.max_heap_size = 64 * 1024 * 1024; // 64 MB max JS heap
        config.min_small_heap_size = 512 * 1024;
        config.idle_gc_enabled = true;
        config.idle_gc_idle_time = 1.0;
        Platform::instance().set_config(config);
        Platform::instance().set_font_loader(GetPlatformFontLoader());
        Platform::instance().set_file_system(GetPlatformFileSystem(String(impl_->web_dir_path.c_str())));

        // 4. Create Renderer
        impl_->renderer = Renderer::Create();
        if (!impl_->renderer) {
            OutputDebugStringA("[WebSurfaceBridge] Failed to create Ultralight Renderer\n");
            return false;
        }

        // 5. Create Offscreen View (350 x 758 px, transparent background for OLED curve)
        ViewConfig view_config;
        view_config.initial_device_scale = 1.0;
        view_config.is_accelerated = false; // CPU BitmapSurface for direct D3D11 upload
        view_config.is_transparent = true;
        view_config.enable_javascript = true;

        impl_->view = impl_->renderer->CreateView(kScreenWidth, kScreenHeight, view_config, nullptr);
        if (!impl_->view) {
            OutputDebugStringA("[WebSurfaceBridge] Failed to create Ultralight View\n");
            return false;
        }

        impl_->view->set_view_listener(impl_.get());
        impl_->view->set_load_listener(impl_.get());

        // 6. Setup Bidirectional JavaScript API Bridge
        RegisterNativeBindings();

        // 7. Load Index Page
        impl_->view->LoadURL("file:///index.html");

        impl_->is_initialized = true;
        OutputDebugStringA("[WebSurfaceBridge] Ultralight initialized successfully\n");
        return true;
    } catch (...) {
        OutputDebugStringA("[WebSurfaceBridge] Exception occurred during Ultralight initialization\n");
        impl_->is_initialized = false;
        return false;
    }
}

void WebSurfaceBridge::Shutdown() {
    if (impl_) {
        impl_->ReleaseD3D();
        impl_->view = nullptr;
        impl_->renderer = nullptr;
        impl_->is_initialized = false;
        impl_->init_attempted = false;
        impl_->is_page_loaded = false;
    }
}

bool WebSurfaceBridge::IsInitialized() const noexcept {
    return impl_ && impl_->is_initialized;
}

bool WebSurfaceBridge::HasTexture() const noexcept {
    return impl_ && impl_->web_srv != nullptr;
}

bool WebSurfaceBridge::IsReady() const noexcept {
    return IsInitialized() && HasTexture();
}

ID3D11ShaderResourceView* WebSurfaceBridge::GetTextureSRV() const noexcept {
    return impl_ ? impl_->web_srv : nullptr;
}

bool WebSurfaceBridge::CreateD3DResources(ID3D11Device* device) {
    if (!device) return false;
    impl_->ReleaseD3D();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = kScreenWidth;
    desc.Height = kScreenHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &impl_->web_texture);
    if (FAILED(hr) || !impl_->web_texture) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(impl_->web_texture, &srv_desc, &impl_->web_srv);
    return SUCCEEDED(hr) && impl_->web_srv != nullptr;
}

void WebSurfaceBridge::ReleaseD3DResources() {
    if (impl_) {
        impl_->ReleaseD3D();
    }
}

void WebSurfaceBridge::UpdateTexture(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!impl_ || !impl_->is_initialized || !device || !context) {
        return;
    }

    if (!impl_->web_texture || !impl_->web_srv) {
        if (!CreateD3DResources(device)) {
            return;
        }
    }

    // Step Ultralight render pipeline
    impl_->renderer->Update();
    impl_->renderer->Render();

    // Transfer rendered pixels from BitmapSurface into D3D11 Texture
    BitmapSurface* surface = static_cast<BitmapSurface*>(impl_->view->surface());
    if (surface) {
        RefPtr<Bitmap> bitmap = surface->bitmap();
        if (bitmap && !bitmap->IsEmpty()) {
            void* pixels = bitmap->LockPixels();
            if (pixels) {
                context->UpdateSubresource(
                    impl_->web_texture,
                    0,
                    nullptr,
                    pixels,
                    bitmap->row_bytes(),
                    0);
                bitmap->UnlockPixels();
            }
        }
    }
}

void WebSurfaceBridge::FireMouseMove(int local_x, int local_y) {
    if (!impl_ || !impl_->view) return;
    impl_->last_mouse_x = local_x;
    impl_->last_mouse_y = local_y;
    MouseEvent evt;
    evt.type = MouseEvent::kType_MouseMoved;
    evt.x = local_x;
    evt.y = local_y;
    evt.button = MouseEvent::kButton_None;
    impl_->view->FireMouseEvent(evt);
}

void WebSurfaceBridge::FireMouseDown(int local_x, int local_y, int button) {
    if (!impl_ || !impl_->view) return;
    MouseEvent evt;
    evt.type = MouseEvent::kType_MouseDown;
    evt.x = local_x;
    evt.y = local_y;
    switch (button) {
        case 1: evt.button = MouseEvent::kButton_Middle; break;
        case 2: evt.button = MouseEvent::kButton_Right; break;
        default: evt.button = MouseEvent::kButton_Left; break;
    }
    impl_->view->FireMouseEvent(evt);
}

void WebSurfaceBridge::FireMouseUp(int local_x, int local_y, int button) {
    if (!impl_ || !impl_->view) return;
    MouseEvent evt;
    evt.type = MouseEvent::kType_MouseUp;
    evt.x = local_x;
    evt.y = local_y;
    switch (button) {
        case 1: evt.button = MouseEvent::kButton_Middle; break;
        case 2: evt.button = MouseEvent::kButton_Right; break;
        default: evt.button = MouseEvent::kButton_Left; break;
    }
    impl_->view->FireMouseEvent(evt);
}

void WebSurfaceBridge::FireMouseScroll(int delta_y) {
    if (!impl_ || !impl_->view) return;
    if (impl_->api) {
        nlohmann::json s_evt;
        s_evt["x"] = impl_->last_mouse_x;
        s_evt["y"] = impl_->last_mouse_y;
        s_evt["delta_y"] = delta_y;
        impl_->api->Emit("scroll", s_evt.dump());
    } else {
        ScrollEvent evt;
        evt.type = ScrollEvent::kType_ScrollByPixel;
        evt.delta_x = 0;
        evt.delta_y = delta_y;
        evt.x = impl_->last_mouse_x;
        evt.y = impl_->last_mouse_y;
        impl_->view->FireScrollEvent(evt);
    }
}

namespace {
std::string LowerAsciiStr(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool IsFleetAtsModel(const core::DriverFleetVehicle& f) {
    const std::string gid = LowerAsciiStr(f.game_id);
    if (gid == "2" || gid == "ats" || gid == "amtrucks") {
        return true;
    }
    const std::string ig = LowerAsciiStr(f.in_game_id);
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

bool IsFleetMatchingActiveGame(const core::DriverFleetVehicle& f, const std::string& active_game_id) {
    const std::string g = LowerAsciiStr(active_game_id);
    const bool is_ats_session = (g == "ats" || g == "amtrucks" || g == "2");
    return IsFleetAtsModel(f) == is_ats_session;
}
} // namespace

void WebSurfaceBridge::DispatchTelemetry() {
    if (!impl_ || !impl_->api || !impl_->view) return;

    try {
        const auto snap = core::TelemetryState::Instance().GetSnapshot();
        const auto account = core::TelemetryState::Instance().GetDriverAccount();

        nlohmann::json j;
        // Truck & Telemetry
        j["speed"] = snap.speed_kmh;
        int active_gear = snap.displayed_gear;
        if (active_gear == 0 && snap.current_gear != 0) {
            active_gear = snap.current_gear;
        }
        j["gear"] = active_gear;
        j["gear_text"] = (active_gear > 0)
                             ? ("D" + std::to_string(active_gear))
                             : ((active_gear < 0) ? ("R" + std::to_string(std::abs(active_gear))) : "N");
        j["rpm"] = snap.engine_rpm;
        j["max_rpm"] = snap.engine_rpm_limit;
        j["fuel"] = snap.fuel_current;
        j["fuel_capacity"] = snap.fuel_capacity;
        j["fuel_range"] = snap.fuel_range_km;
        j["fuel_avg_consumption"] = snap.fuel_avg_consumption;
        j["fuel_warning"] = snap.fuel_warning;
        j["odometer"] = snap.odometer_km;
        j["cruise_control"] = snap.cruise_control_kmh;
        j["park_brake"] = snap.parking_brake;
        j["motor_brake"] = snap.motor_brake;
        j["retarder_level"] = snap.retarder_level;
        j["light_high_beam"] = snap.light_high_beam;
        j["hazard_warning"] = snap.hazard_warning;
        j["differential_lock"] = snap.differential_lock;
        j["electric_enabled"] = snap.electric_enabled;
        j["engine_enabled"] = snap.engine_running;
        j["speed_limit_kmh"] = snap.speed_limit_kmh;
        j["heading_deg"] = snap.heading_deg;
        j["pos_x"] = snap.pos_x;
        j["pos_y"] = snap.pos_y;
        j["pos_z"] = snap.pos_z;
        j["trailer_connected"] = snap.trailer_connected;

        // Powertrain metrics
        j["water_temperature"] = snap.water_temperature_c;
        j["oil_temperature"] = snap.oil_temperature_c;
        j["oil_pressure"] = 4.2;
        j["turbo_pressure"] = 1.1;
        j["battery_voltage"] = snap.battery_voltage;
        j["brake_air_psi"] = snap.brake_air_pressure_psi;
        j["air_pressure"] = snap.brake_air_pressure_psi * 0.0689476; // convert psi to bar

        // Simulation & SDK Status
        j["sdk_active"] = snap.sdk_active;
        j["is_paused"] = snap.is_paused;

        // Navigation & Job
        const bool effective_job_active = snap.job_active || !snap.active_job_db_id.empty();
        const float driven_km = (effective_job_active && snap.odometer_km >= snap.job_start_odometer_km)
                                    ? (snap.odometer_km - snap.job_start_odometer_km)
                                    : 0.0f;
        j["job_active"] = effective_job_active;
        j["is_resumed_job"] = (snap.active_job_sync_status.find("Resumed") != std::string::npos);
        j["cargo"] = snap.cargo_name;
        j["cargo_mass_kg"] = snap.cargo_mass_kg;
        j["city_src"] = snap.origin_city;
        j["company_src"] = snap.origin_company;
        j["city_dst"] = snap.destination_city;
        j["company_dst"] = snap.destination_company;
        j["distance_km"] = driven_km;
        j["planned_distance_km"] = snap.planned_distance_km;
        j["nav_distance_km"] = snap.navigation_distance_m / 1000.0f;
        j["job_market"] = snap.job_market;
        j["is_special_job"] = (snap.is_special_job || snap.active_job_is_special);
        j["active_job_db_id"] = snap.active_job_db_id;
        j["active_job_locked_price"] = snap.active_job_locked_price;
        j["active_job_sync_status"] = snap.active_job_sync_status;
        j["job_fuel_consumed_liters"] = snap.job_fuel_consumed_liters;
        j["job_max_speed_kmh"] = snap.job_max_speed_kmh;
        j["job_fines_count"] = static_cast<int32_t>(snap.job_fines.size());
        j["job_accum_vehicle_damage"] = snap.job_accum_vehicle_damage;
        j["job_accum_trailer_damage"] = snap.job_accum_trailer_damage;
        j["job_accum_cargo_damage"] = snap.job_accum_cargo_damage;
        j["contract_deadline_min"] = snap.contract_deadline_min;
        j["reward_payout"] = snap.reward_payout;
        j["job_start_timestamp_sec"] = snap.job_start_timestamp_sec;
        j["hardcore_speeding_seconds"] = snap.hardcore_speeding_seconds;
        j["hardcore_rest_overdue_minutes"] = snap.hardcore_rest_overdue_minutes;
        j["hardcore_auto_park_used"] = snap.hardcore_auto_park_used;
        j["hardcore_warp_detected"] = snap.hardcore_warp_detected;
        j["hardcore_eligible_at_start"] = snap.hardcore_eligible_at_start;
        j["hardcore_disqualified"] = snap.hardcore_disqualified;
        j["hardcore_disqualify_reason"] = snap.hardcore_disqualify_reason;

        // Last Job Paycheck Receipt
        j["has_last_job_receipt"] = snap.has_last_job_receipt;
        j["last_receipt_job_id"] = snap.last_receipt_job_id;
        j["last_receipt_game_mode"] = snap.last_receipt_game_mode;
        j["last_receipt_distance_km"] = snap.last_receipt_distance_km;
        j["last_receipt_revenue_nc"] = snap.last_receipt_revenue_nc;
        j["last_receipt_base_nc"] = snap.last_receipt_base_nc;
        j["last_receipt_mp_bonus_nc"] = snap.last_receipt_mp_bonus_nc;
        j["last_receipt_hardcore_nc"] = snap.last_receipt_hardcore_nc;
        j["last_receipt_nplus_nc"] = snap.last_receipt_nplus_nc;
        j["last_receipt_cost_nc"] = snap.last_receipt_cost_nc;
        j["last_receipt_tax_nc"] = snap.last_receipt_tax_nc;
        j["last_receipt_xp"] = snap.last_receipt_xp;
        j["last_receipt_penalty_pts"] = snap.last_receipt_penalty_pts;
        j["last_receipt_is_hardcore"] = snap.last_receipt_is_hardcore;
        j["last_receipt_hardcore_points"] = snap.last_receipt_hardcore_points;
        j["last_receipt_stars_distance"] = snap.last_receipt_stars_distance;
        j["last_receipt_stars_mass"] = snap.last_receipt_stars_mass;
        j["last_receipt_stars_damage"] = snap.last_receipt_stars_damage;
        j["last_receipt_stars_fuel"] = snap.last_receipt_stars_fuel;
        j["last_receipt_stars_fines"] = snap.last_receipt_stars_fines;
        j["last_receipt_stars_total"] = snap.last_receipt_stars_total;
        j["last_receipt_disqualify_reason"] = snap.last_receipt_disqualify_reason;

        // Damage
        j["wear_engine"] = snap.wear_engine;
        j["wear_transmission"] = snap.wear_transmission;
        j["wear_cabin"] = snap.wear_cabin;
        j["wear_chassis"] = snap.wear_chassis;
        j["wear_wheels"] = snap.wear_wheels;
        j["wear_trailer"] = snap.wear_trailer;
        j["cargo_damage"] = snap.cargo_damage;

        // Account & Economy
        j["account_authenticated"] = (account.status == core::AccountSyncStatus::Authenticated);
        j["account_status_message"] = account.status_message;
        j["last_sync_timestamp"] = account.last_sync_timestamp;
        j["driver_name"] = account.name;
        j["driver_avatar"] = account.image_url;
        j["driver_banner"] = account.banner_url;
        j["rank"] = account.trucky_rank;
        j["rank_color"] = account.trucky_rank_color;
        j["role"] = account.trucky_role;
        j["role_color"] = account.trucky_role_color;
        j["total_nc"] = account.total_nc;
        j["total_exp"] = account.xp;
        j["level"] = account.level;
        j["joined_convoy"] = account.joined_convoy;
        j["penalty_points"] = account.penalty_points;
        j["point_price"] = account.point_price;
        j["discount_booster"] = account.discount_booster;
        j["discount_nismara_plus"] = account.discount_nismara_plus;
        j["total_discount"] = account.total_discount;
        j["final_point_price"] = account.final_point_price;
        const int32_t safebox_avail =
            account.garage.safebox_stock > 0 ? account.garage.safebox_stock : account.safebox_tickets;
        const int32_t safebox_cap =
            account.garage.max_penalty_tickets > 0 ? account.garage.max_penalty_tickets : account.max_penalty_tickets;
        j["safebox_tickets"] = safebox_avail;
        j["max_penalty_tickets"] = safebox_cap;
        j["penalty_action_message"] = account.penalty_action_message;
        j["penalty_action_is_error"] = account.penalty_action_is_error;
        j["server_name"] = snap.server_name;
        j["server_mode"] = snap.server_mode;
        j["nismara_plus_active"] = account.nismara_plus_active;
        j["nismara_plus_expired_at"] = account.nismara_plus_expired_at;
        j["is_booster"] = account.is_booster;
        j["insurance_active"] = account.insurance_active;
        j["insurance_rating"] = account.insurance_rating;
        j["insurance_expired_at"] = account.insurance_expired_at;
        j["nc_boost_active"] = account.nc_boost_active;
        j["nc_boost_multiplier"] = account.nc_boost_multiplier;
        j["nc_boost_voucher_title"] = account.nc_boost_voucher_title;
        j["top_manager_active"] = account.top_manager_active;
        j["top_manager_month"] = account.top_manager_month;
        j["discord_id"] = account.discord_id;
        j["steam_id"] = account.steam_id;

        // System Config & Hardcore Diagnostics (9 Parameters + Mod Check)
        const auto cfg = core::ConfigManager::Instance().GetConfig();
        j["auto_lock_on_close"] = cfg.auto_lock_on_close;
        j["phone_frame_color"] = cfg.phone_frame_color;
        j["driver_token"] = cfg.driver_token;
        j["api_base_url"] = cfg.api_base_url;
#ifndef NISMARA_VERSION
#define NISMARA_VERSION "1.0.3-beta"
#endif
        j["plugin_version"] = NISMARA_VERSION;
        j["os_version"] = std::string("Nismara OS v") + NISMARA_VERSION;
        j["build_version"] = std::string("Build ") + NISMARA_VERSION + "-x64";
        j["game_id"] = snap.game_id;
        j["game_name"] = snap.game_name;
        j["overlay_open_seq"] = TabletGui::Instance().GetOpenSequence();

        j["hc_traffic_ok"] = snap.hc_traffic_ok;
        j["hc_fatigue_ok"] = snap.hc_fatigue_ok;
        j["hc_police_ok"] = snap.hc_police_ok;
        j["hc_parking_ok"] = snap.hc_parking_ok;
        j["hc_road_events_ok"] = snap.hc_road_events_ok;
        j["hc_rain_prob_ok"] = snap.hc_rain_prob_ok;
        j["hc_advanced_coupling_ok"] = snap.hc_advanced_coupling_ok;
        j["hc_fuel_simulation_ok"] = snap.hc_fuel_simulation_ok;
        j["hc_air_brake_ok"] = snap.hc_air_brake_ok;
        j["hc_forbidden_mods_found"] = snap.hc_forbidden_mods_found;
        j["hc_forbidden_mods_list"] = snap.hc_forbidden_mods_list;

        // Truck Identification
        j["truck_name"] = snap.truck_name;
        j["truck_brand"] = snap.truck_brand;
        j["truck_in_game_id"] = snap.truck_in_game_id;
        j["truck_model_id"] = snap.truck_model_id;
        j["truck_brand_id"] = snap.truck_brand_id;

        std::string fleet_num = "";
        if (account.active_fleet_index >= 0 &&
            account.active_fleet_index < static_cast<int>(account.fleets.size())) {
            fleet_num = account.fleets[account.active_fleet_index].fleet_number;
        } else if (!account.manual_override_fleet_number.empty()) {
            fleet_num = account.manual_override_fleet_number;
        }
        j["fleet_number"] = fleet_num;
        j["has_matched_fleet"] = !fleet_num.empty();
        j["active_fleet_index"] = account.active_fleet_index;

        // Fleets array (Filtered strictly by active game: ETS2 vs ATS)
        nlohmann::json fleets_arr = nlohmann::json::array();
        for (const auto& fl : account.fleets) {
            if (!IsFleetMatchingActiveGame(fl, snap.game_id)) {
                continue;
            }
            nlohmann::json f_item;
            f_item["fleet_number"] = fl.fleet_number;
            f_item["fleet_name"] = fl.fleet_name;
            f_item["custom_name"] = fl.custom_name;
            f_item["brand_name"] = fl.brand_name;
            f_item["model_name"] = fl.model_name;
            f_item["model_type"] = fl.model_type;
            f_item["in_game_id"] = fl.in_game_id;
            f_item["game_id"] = IsFleetAtsModel(fl) ? "2" : "1";
            f_item["photo_url"] = fl.photo_url;
            f_item["has_insurance"] = fl.has_insurance;
            f_item["wheels"] = fl.wheels;
            f_item["odometer"] = fl.odometer;
            f_item["status"] = fl.status;
            f_item["maintenance_needed"] = fl.maintenance_needed;
            f_item["maintenance_start_date"] = fl.maintenance_start_date;
            f_item["maintenance_end_date"] = fl.maintenance_end_date;
            f_item["jobs_count"] = fl.jobs_count;
            f_item["total_revenue"] = fl.total_revenue;
            f_item["total_fuel_used"] = fl.total_fuel_used;
            f_item["total_damage_cost"] = fl.total_damage_cost;
            f_item["unfix_engine"] = fl.unfix_engine;
            f_item["unfix_transmission"] = fl.unfix_transmission;
            f_item["unfix_brakes"] = fl.unfix_brakes;
            f_item["unfix_tires"] = fl.unfix_tires;
            f_item["maint_engine"] = fl.maint_engine;
            f_item["maint_tires"] = fl.maint_tires;
            f_item["maint_transmission"] = fl.maint_transmission;
            f_item["maint_brakes"] = fl.maint_brakes;
            f_item["last_maint_engine"] = fl.last_maint_engine;
            f_item["last_maint_tires"] = fl.last_maint_tires;
            f_item["last_maint_transmission"] = fl.last_maint_transmission;
            f_item["last_maint_brakes"] = fl.last_maint_brakes;
            fleets_arr.push_back(f_item);
        }
        j["fleets"] = fleets_arr;

        // Garage summary
        nlohmann::json g_obj;
        g_obj["has_garage"] = account.garage.has_garage;
        g_obj["status"] = account.garage.status;
        g_obj["fuel_stock"] = account.garage.fuel_stock;
        g_obj["fuel_capacity"] = account.garage.fuel_capacity;
        g_obj["fuel_tank_level"] = account.garage.fuel_tank_level;
        g_obj["fuel_level"] = account.garage.fuel_tank_level;
        g_obj["fleet_slot"] = account.garage.fleet_slot;
        g_obj["fleet_slot_used"] = account.garage.fleet_slot_used;
        g_obj["fleet_slot_level"] = account.garage.fleet_slot_level;
        g_obj["fleet_level"] = account.garage.fleet_slot_level;
        g_obj["safebox_stock"] = account.garage.safebox_stock;
        g_obj["safebox_level"] = account.garage.safebox_level;
        g_obj["max_penalty_tickets"] = account.garage.max_penalty_tickets;
        g_obj["operational_cost"] = account.garage.operational_cost;
        g_obj["fleet_operational_cost"] = account.garage.fleet_operational_cost;
        g_obj["fuel_operational_cost"] = account.garage.fuel_operational_cost;
        g_obj["safebox_operational_cost"] = account.garage.safebox_operational_cost;
        g_obj["next_payment_date"] = account.garage.next_payment_date;
        j["garage"] = g_obj;

        std::string json_str = j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        impl_->api->Emit("telemetry", json_str);
    } catch (const std::exception& ex) {
        OutputDebugStringA(("[WebSurfaceBridge] DispatchTelemetry std::exception: " + std::string(ex.what()) + "\n").c_str());
    } catch (...) {
        OutputDebugStringA("[WebSurfaceBridge] DispatchTelemetry unknown exception\n");
    }
}

void WebSurfaceBridge::Reload() {
    if (impl_ && impl_->view) {
        impl_->view->LoadURL("file:///index.html");
    }
}

void WebSurfaceBridge::RegisterNativeBindings() {
    impl_->api = std::make_unique<js::API>("nismara");

    // Version
    (*impl_->api)["version"] = "2.0.0-react";

    // Native Functions
    impl_->api->Bind("lockPhone", []() {
        TabletGui::Instance().LockPhone();
    });

    impl_->api->Bind("unlockPhone", []() {
        TabletGui::Instance().UnlockToHome();
    });

    impl_->api->Bind("closePhone", []() {
        TabletGui::Instance().SetVisible(false);
    });

    impl_->api->Bind("openApp", [](int app_id) {
        TabletGui::Instance().OpenApp(static_cast<PhoneAppId>(app_id));
    }, js::Param("appId"));

    impl_->api->Bind("syncAccount", []() {
        core::ApiClient::Instance().SyncAccountAsync();
    });

    impl_->api->Bind("selectFleet", [](std::string fleet_number) {
        core::TelemetryState::Instance().SetManualFleetOverride(fleet_number);
    }, js::Param("fleetNumber"));

    impl_->api->Bind("payPenalty", [](int points) {
        core::ApiClient::Instance().PayPenaltyPointsAsync(points);
    }, js::Param("points"));

    impl_->api->Bind("useTicket", [](int tickets) {
        core::ApiClient::Instance().UsePenaltyTicketAsync(tickets);
    }, js::Param("tickets"));

    impl_->api->Bind("rescanProfile", []() {
        const auto game_docs = core::ConfigManager::Instance().GetGameUserDataDirectory();
        const auto hc_status = core::ProfileValidator::ScanProfileSettings(game_docs);
        core::TelemetryState::Instance().Write([&hc_status](core::TelemetrySnapshot& s) {
            s.hardcore_profile_scanned = true;
            s.hc_traffic_ok = hc_status.traffic_ok;
            s.hc_fatigue_ok = hc_status.fatigue_ok;
            s.hc_police_ok = hc_status.police_ok;
            s.hc_parking_ok = hc_status.parking_ok;
            s.hc_road_events_ok = hc_status.road_events_ok;
            s.hc_rain_prob_ok = hc_status.rain_prob_ok;
            s.hc_advanced_coupling_ok = hc_status.advanced_coupling_ok;
            s.hc_fuel_simulation_ok = hc_status.fuel_simulation_ok;
            s.hc_air_brake_ok = hc_status.air_brake_ok;
            s.hc_forbidden_mods_found = hc_status.has_forbidden_mods;
            s.hc_forbidden_mods_list.clear();
            if (hc_status.has_forbidden_mods && !hc_status.forbidden_mods.empty()) {
                for (size_t i = 0; i < hc_status.forbidden_mods.size(); ++i) {
                    if (i > 0) s.hc_forbidden_mods_list += ", ";
                    s.hc_forbidden_mods_list += hc_status.forbidden_mods[i];
                }
            }
        });
    });

    impl_->api->Bind("setAutoLock", [](bool enabled) {
        core::ConfigManager::Instance().SetAutoLockOnClose(enabled);
        core::ConfigManager::Instance().Save();
    }, js::Param("enabled"));

    impl_->api->Bind("setFrameColor", [](int color_idx) {
        if (color_idx >= 0 && color_idx < 5) {
            core::ConfigManager::Instance().SetPhoneFrameColor(color_idx);
            core::ConfigManager::Instance().Save();
        }
    }, js::Param("colorIdx"));

    // Event definition for telemetry streaming
    impl_->api->DefineEvent<std::string>("telemetry", js::Param("data"));

    // Attach to view
    (void)impl_->api->AttachTo(impl_->view.get());
}

} // namespace nismara::ui
