#include "hooks/dx11_hook.hpp"
#include "hooks/wndproc_hook.hpp"
#include "ui/tablet_gui.hpp"
#include "ui/texture_manager.hpp"
#include "ui/web_surface_bridge.hpp"

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <chrono>
#include <cstring>

namespace nismara::hooks {

Dx11Hook& Dx11Hook::Instance() {
    static Dx11Hook instance;
    return instance;
}

void Dx11Hook::StartAsync() {
    if (started_.exchange(true)) {
        return;
    }
    stop_requested_.store(false);
    shutting_down_.store(false);
    worker_thread_ = std::thread(&Dx11Hook::WorkerThreadProc, this);
}

void Dx11Hook::Shutdown() {
    if (!started_.exchange(false)) {
        return;
    }
    shutting_down_.store(true);
    stop_requested_.store(true);

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // 1. Restore WndProc first so window messages bypass ImGui
    WndProcHook::Instance().Shutdown();

    // 2. Disable and remove MinHook detours
    if (hook_installed_.exchange(false)) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }

    // 3. Tear down Dear ImGui context, WebSurfaceBridge, and DX11 render target resources
    nismara::ui::WebSurfaceBridge::Instance().Shutdown();
    nismara::ui::TextureManager::Instance().ReleaseAll();
    if (imgui_initialized_.exchange(false)) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    CleanupRenderTarget();

    if (context_ != nullptr) {
        context_->Release();
        context_ = nullptr;
    }
    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

namespace {

inline HRESULT SafeCallPresent(
    Dx11Hook::PresentFn fn,
    IDXGISwapChain* swap_chain,
    UINT sync_interval,
    UINT flags) {
    __try {
        return fn(swap_chain, sync_interval, flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return S_OK;
    }
}

inline HRESULT SafeCallResizeBuffers(
    Dx11Hook::ResizeBuffersFn fn,
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT new_format,
    UINT swap_chain_flags) {
    __try {
        return fn(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return S_OK;
    }
}

inline BOOL SafeCallSetCursorPos(Dx11Hook::SetCursorPosFn fn, int x, int y) {
    __try {
        return fn(x, y);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return TRUE;
    }
}

inline BOOL SafeCallClipCursor(Dx11Hook::ClipCursorFn fn, const RECT* rect) {
    __try {
        return fn(rect);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return TRUE;
    }
}

inline HRESULT SafeCallDIAcquire(Dx11Hook::DIAcquireFn fn, IDirectInputDevice8W* device) {
    __try {
        return fn(device);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return DI_OK;
    }
}

inline HRESULT SafeCallDIUnacquire(Dx11Hook::DIUnacquireFn fn, IDirectInputDevice8W* device) {
    __try {
        return fn(device);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return DI_OK;
    }
}

inline HRESULT SafeCallDIGetDeviceState(
    Dx11Hook::DIGetDeviceStateFn fn,
    IDirectInputDevice8W* device,
    DWORD cb_data,
    LPVOID lpv_data) {
    __try {
        return fn(device, cb_data, lpv_data);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return DI_OK;
    }
}

inline HRESULT SafeCallDIGetDeviceData(
    Dx11Hook::DIGetDeviceDataFn fn,
    IDirectInputDevice8W* device,
    DWORD cb_object_data,
    LPDIDEVICEOBJECTDATA rgdod,
    LPDWORD pdw_in_out,
    DWORD dw_flags) {
    __try {
        return fn(device, cb_object_data, rgdod, pdw_in_out, dw_flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return DI_OK;
    }
}

inline bool SafeCheckMouseCaps(IDirectInputDevice8W* device, DIDEVCAPS* caps) {
    __try {
        return SUCCEEDED(device->GetCapabilities(caps));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

void Dx11Hook::OnDeviceChange() {
    last_device_change_tick_.store(GetTickCount64());
    active_mouse_device_.store(nullptr);
    mouse_unacquired_for_overlay_.store(false);
    for (size_t i = 0; i < kMaxCachedNonMouseDevices; ++i) {
        non_mouse_devices_[i].store(nullptr);
    }

    if (nismara::ui::TabletGui::Instance().IsVisible()) {
        nismara::ui::TabletGui::Instance().SetVisible(false);
        last_frame_visible_ = false;
        if (original_clip_cursor_ != nullptr) {
            SafeCallClipCursor(original_clip_cursor_, nullptr);
        }
        CleanupRenderTarget();
    }
}

void Dx11Hook::OnOverlayVisibilityChanged(bool visible) {
    if (visible) {
        // Release any Win32 cursor clipping and mouse capture held by the game
        if (original_clip_cursor_ != nullptr) {
            SafeCallClipCursor(original_clip_cursor_, nullptr);
        } else {
            ClipCursor(nullptr);
        }
        ReleaseCapture();

        // Unacquire DirectInput8 mouse device so Windows unlocks OS cursor movement
        IDirectInputDevice8W* mouse_dev = active_mouse_device_.load();
        if (mouse_dev != nullptr && original_di_unacquire_ != nullptr) {
            SafeCallDIUnacquire(original_di_unacquire_, mouse_dev);
            mouse_unacquired_for_overlay_.store(true);
        }

        // Restore or center cursor inside the game window
        if (game_hwnd_ != nullptr && IsWindow(game_hwnd_)) {
            POINT target_pt = saved_overlay_cursor_pos_;
            if (target_pt.x < 0 || target_pt.y < 0) {
                RECT rc = {};
                if (GetClientRect(game_hwnd_, &rc)) {
                    target_pt.x = (rc.right - rc.left) / 2;
                    target_pt.y = (rc.bottom - rc.top) / 2;
                    ClientToScreen(game_hwnd_, &target_pt);
                }
            }
            if (target_pt.x >= 0 && target_pt.y >= 0) {
                if (original_set_cursor_pos_ != nullptr) {
                    SafeCallSetCursorPos(original_set_cursor_pos_, target_pt.x, target_pt.y);
                } else {
                    SetCursorPos(target_pt.x, target_pt.y);
                }
            }
        }
    } else {
        // Save current cursor position for the next time the tablet is opened
        GetCursorPos(&saved_overlay_cursor_pos_);

        // Re-acquire DirectInput8 mouse device so the game immediately regains mouse look/steering
        IDirectInputDevice8W* mouse_dev = active_mouse_device_.load();
        if (mouse_dev != nullptr && original_di_acquire_ != nullptr &&
            mouse_unacquired_for_overlay_.exchange(false)) {
            SafeCallDIAcquire(original_di_acquire_, mouse_dev);
        }
    }
}

void Dx11Hook::WorkerThreadProc() {
    try {
        // Enforce initial delay of 4 seconds (40 x 100ms) to avoid race conditions
        // with TruckersMP, DihaulMP, and Steam overlays during startup.
        for (int i = 0; i < 40; ++i) {
            if (stop_requested_.load()) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        void* present_addr = nullptr;
        void* resize_addr = nullptr;

        // Retry extracting dummy swapchain up to 10 times if the game window/DX11 is still initializing
        for (int attempt = 0; attempt < 10 && !stop_requested_.load(); ++attempt) {
            if (ExtractSwapChainVTable(&present_addr, &resize_addr)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (stop_requested_.load() || present_addr == nullptr) {
            return;
        }

        if (MH_Initialize() != MH_OK) {
            return;
        }

        // 1. Hook IDXGISwapChain::Present and IDXGISwapChain::ResizeBuffers
        if (MH_CreateHook(
                present_addr,
                reinterpret_cast<LPVOID>(&Dx11Hook::HookedPresent),
                reinterpret_cast<LPVOID*>(&original_present_)) != MH_OK) {
            MH_Uninitialize();
            return;
        }

        if (resize_addr != nullptr) {
            MH_CreateHook(
                resize_addr,
                reinterpret_cast<LPVOID>(&Dx11Hook::HookedResizeBuffers),
                reinterpret_cast<LPVOID*>(&original_resize_buffers_));
        }

        // 2. Hook user32.dll!SetCursorPos and user32.dll!ClipCursor so ETS2/ATS cannot
        //    force the OS cursor back to the center of the window while the tablet is open.
        HMODULE user32_mod = GetModuleHandleW(L"user32.dll");
        if (user32_mod != nullptr) {
            void* set_cursor_pos_addr = reinterpret_cast<void*>(GetProcAddress(user32_mod, "SetCursorPos"));
            if (set_cursor_pos_addr != nullptr) {
                MH_CreateHook(
                    set_cursor_pos_addr,
                    reinterpret_cast<LPVOID>(&Dx11Hook::HookedSetCursorPos),
                    reinterpret_cast<LPVOID*>(&original_set_cursor_pos_));
            }

            void* clip_cursor_addr = reinterpret_cast<void*>(GetProcAddress(user32_mod, "ClipCursor"));
            if (clip_cursor_addr != nullptr) {
                MH_CreateHook(
                    clip_cursor_addr,
                    reinterpret_cast<LPVOID>(&Dx11Hook::HookedClipCursor),
                    reinterpret_cast<LPVOID*>(&original_clip_cursor_));
            }
        }

        // 3. Hook DirectInput8 (IDirectInputDevice8W) so exclusive mouse lock is released
        //    and cabin camera rotation is suppressed while interacting with the tablet.
        void* di_acquire_addr = nullptr;
        void* di_unacquire_addr = nullptr;
        void* di_get_state_addr = nullptr;
        void* di_get_data_addr = nullptr;
        if (ExtractDirectInputVTable(&di_acquire_addr, &di_unacquire_addr, &di_get_state_addr, &di_get_data_addr)) {
            original_di_unacquire_ = reinterpret_cast<DIUnacquireFn>(di_unacquire_addr);

            if (di_acquire_addr != nullptr) {
                MH_CreateHook(
                    di_acquire_addr,
                    reinterpret_cast<LPVOID>(&Dx11Hook::HookedDIAcquire),
                    reinterpret_cast<LPVOID*>(&original_di_acquire_));
            }
            if (di_get_state_addr != nullptr) {
                MH_CreateHook(
                    di_get_state_addr,
                    reinterpret_cast<LPVOID>(&Dx11Hook::HookedDIGetDeviceState),
                    reinterpret_cast<LPVOID*>(&original_di_get_device_state_));
            }
            if (di_get_data_addr != nullptr) {
                MH_CreateHook(
                    di_get_data_addr,
                    reinterpret_cast<LPVOID>(&Dx11Hook::HookedDIGetDeviceData),
                    reinterpret_cast<LPVOID*>(&original_di_get_device_data_));
            }
        }

        if (MH_EnableHook(MH_ALL_HOOKS) == MH_OK) {
            hook_installed_.store(true);
        } else {
            MH_Uninitialize();
        }
    } catch (...) {
        // Must never allow background thread exception to terminate host game
    }
}

bool Dx11Hook::ExtractSwapChainVTable(void** out_present, void** out_resize_buffers) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"NismaraDummyDX11WndClass";

    RegisterClassExW(&wc);

    HWND dummy_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"NismaraDummyDX11Wnd",
        WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if (dummy_hwnd == nullptr) {
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 2;
    sd.BufferDesc.Height = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = dummy_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* dummy_swap_chain = nullptr;
    ID3D11Device* dummy_device = nullptr;
    ID3D11DeviceContext* dummy_context = nullptr;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_levels,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &dummy_swap_chain,
        &dummy_device,
        &feature_level,
        &dummy_context);

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            feature_levels,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &dummy_swap_chain,
            &dummy_device,
            &feature_level,
            &dummy_context);
    }

    bool success = false;
    if (SUCCEEDED(hr) && dummy_swap_chain != nullptr) {
        void** vtable = *reinterpret_cast<void***>(dummy_swap_chain);
        *out_present = vtable[8];         // IDXGISwapChain::Present
        *out_resize_buffers = vtable[13]; // IDXGISwapChain::ResizeBuffers
        success = true;
    }

    if (dummy_swap_chain != nullptr) {
        dummy_swap_chain->Release();
    }
    if (dummy_context != nullptr) {
        dummy_context->Release();
    }
    if (dummy_device != nullptr) {
        dummy_device->Release();
    }

    DestroyWindow(dummy_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return success;
}

bool Dx11Hook::ExtractDirectInputVTable(
    void** out_acquire,
    void** out_unacquire,
    void** out_get_device_state,
    void** out_get_device_data) {
    HMODULE dinput8_mod = GetModuleHandleW(L"dinput8.dll");
    if (dinput8_mod == nullptr) {
        dinput8_mod = LoadLibraryW(L"dinput8.dll");
    }
    if (dinput8_mod == nullptr) {
        return false;
    }

    using DirectInput8CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    auto di8_create = reinterpret_cast<DirectInput8CreateFn>(GetProcAddress(dinput8_mod, "DirectInput8Create"));
    if (di8_create == nullptr) {
        return false;
    }

    IDirectInput8W* dinput = nullptr;
    if (FAILED(di8_create(
            GetModuleHandleW(nullptr),
            DIRECTINPUT_VERSION,
            IID_IDirectInput8W,
            reinterpret_cast<LPVOID*>(&dinput),
            nullptr)) ||
        dinput == nullptr) {
        return false;
    }

    IDirectInputDevice8W* dummy_mouse = nullptr;
    bool success = false;
    if (SUCCEEDED(dinput->CreateDevice(GUID_SysMouse, &dummy_mouse, nullptr)) && dummy_mouse != nullptr) {
        void** vtable = *reinterpret_cast<void***>(dummy_mouse);
        *out_acquire = vtable[7];           // IDirectInputDevice8W::Acquire
        *out_unacquire = vtable[8];         // IDirectInputDevice8W::Unacquire
        *out_get_device_state = vtable[9];  // IDirectInputDevice8W::GetDeviceState
        *out_get_device_data = vtable[10];  // IDirectInputDevice8W::GetDeviceData
        dummy_mouse->Release();
        success = true;
    }

    dinput->Release();
    return success;
}

void Dx11Hook::MarkNonMouseDevice(IDirectInputDevice8W* device) {
    if (device == nullptr) {
        return;
    }
    auto& self = Instance();
    for (size_t i = 0; i < kMaxCachedNonMouseDevices; ++i) {
        IDirectInputDevice8W* cur = self.non_mouse_devices_[i].load();
        if (cur == device) {
            return;
        }
        if (cur == nullptr) {
            IDirectInputDevice8W* expected = nullptr;
            if (self.non_mouse_devices_[i].compare_exchange_strong(expected, device)) {
                return;
            }
        }
    }
}

bool Dx11Hook::IsKnownNonMouseDevice(IDirectInputDevice8W* device) {
    if (device == nullptr) {
        return true;
    }
    auto& self = Instance();
    for (size_t i = 0; i < kMaxCachedNonMouseDevices; ++i) {
        if (self.non_mouse_devices_[i].load() == device) {
            return true;
        }
    }
    return false;
}

bool Dx11Hook::IsMouseDevice(IDirectInputDevice8W* device) {
    if (device == nullptr) {
        return false;
    }
    auto& self = Instance();
    if (device == self.active_mouse_device_.load()) {
        return true;
    }
    if (IsKnownNonMouseDevice(device)) {
        return false;
    }
    // Never call GetCapabilities while ETS2's [di8] subsystem is handling WM_DEVICECHANGE
    if (GetTickCount64() - self.last_device_change_tick_.load() < 2000ULL) {
        return false;
    }

    DIDEVCAPS caps = {};
    caps.dwSize = sizeof(DIDEVCAPS);
    if (SafeCheckMouseCaps(device, &caps)) {
        const BYTE dev_type = LOBYTE(caps.dwDevType);
        // DI8DEVTYPE_MOUSE (0x12) or legacy DIDEVTYPE_MOUSE (2)
        if (dev_type == DI8DEVTYPE_MOUSE || dev_type == 2) {
            self.active_mouse_device_.store(device);
            return true;
        }
        // Cache keyboard/joystick/wheel/gamepad so we never call GetCapabilities on it again
        MarkNonMouseDevice(device);
    }
    return false;
}

void Dx11Hook::CreateRenderTarget(IDXGISwapChain* swap_chain) {
    if (swap_chain == nullptr || device_ == nullptr || main_rtv_ != nullptr) {
        return;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    if (SUCCEEDED(swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer))) &&
        back_buffer != nullptr) {
        device_->CreateRenderTargetView(back_buffer, nullptr, &main_rtv_);
        back_buffer->Release();
    }
}

void Dx11Hook::CleanupRenderTarget() {
    if (main_rtv_ != nullptr) {
        if (context_ != nullptr) {
            ID3D11RenderTargetView* bound_rtv = nullptr;
            ID3D11DepthStencilView* bound_dsv = nullptr;
            context_->OMGetRenderTargets(1, &bound_rtv, &bound_dsv);
            if (bound_rtv == main_rtv_) {
                ID3D11RenderTargetView* null_views[1] = {nullptr};
                context_->OMSetRenderTargets(1, null_views, bound_dsv);
            }
            if (bound_rtv != nullptr) bound_rtv->Release();
            if (bound_dsv != nullptr) bound_dsv->Release();
        }
        main_rtv_->Release();
        main_rtv_ = nullptr;
    }
}

void Dx11Hook::InitializeImGui(IDXGISwapChain* swap_chain) {
    if (imgui_initialized_.load() || swap_chain == nullptr) {
        return;
    }

    if (FAILED(swap_chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device_))) ||
        device_ == nullptr) {
        return;
    }

    device_->GetImmediateContext(&context_);

    DXGI_SWAP_CHAIN_DESC sd = {};
    swap_chain->GetDesc(&sd);
    game_hwnd_ = sd.OutputWindow;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    nismara::ui::TabletGui::Instance().InitializeStyle();
    nismara::ui::TextureManager::Instance().SetDevice(device_);

    ImGui_ImplWin32_Init(game_hwnd_);
    ImGui_ImplDX11_Init(device_, context_);

    WndProcHook::Instance().Install(game_hwnd_);

    imgui_initialized_.store(true);
}

BOOL WINAPI Dx11Hook::HookedSetCursorPos(int x, int y) {
    auto& self = Dx11Hook::Instance();
    if (!self.shutting_down_.load() && nismara::ui::TabletGui::Instance().IsVisible()) {
        // Prevent the game from resetting the OS cursor back to the center of the window
        return TRUE;
    }
    if (self.original_set_cursor_pos_ != nullptr) {
        return SafeCallSetCursorPos(self.original_set_cursor_pos_, x, y);
    }
    return SetCursorPos(x, y);
}

BOOL WINAPI Dx11Hook::HookedClipCursor(const RECT* rect) {
    auto& self = Dx11Hook::Instance();
    if (!self.shutting_down_.load() && nismara::ui::TabletGui::Instance().IsVisible()) {
        // Keep cursor unclipped while tablet overlay is open
        if (self.original_clip_cursor_ != nullptr) {
            SafeCallClipCursor(self.original_clip_cursor_, nullptr);
        }
        return ClipCursor(nullptr);
    }
    if (self.original_clip_cursor_ != nullptr) {
        return SafeCallClipCursor(self.original_clip_cursor_, rect);
    }
    return ClipCursor(rect);
}

HRESULT STDMETHODCALLTYPE Dx11Hook::HookedDIAcquire(IDirectInputDevice8W* device) {
    auto& self = Dx11Hook::Instance();
    if (!self.shutting_down_.load() &&
        (GetTickCount64() - self.last_device_change_tick_.load() >= 2000ULL) &&
        nismara::ui::TabletGui::Instance().IsVisible() &&
        IsMouseDevice(device)) {
        return DI_OK;
    }
    if (self.original_di_acquire_ != nullptr) {
        return SafeCallDIAcquire(self.original_di_acquire_, device);
    }
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE Dx11Hook::HookedDIGetDeviceState(
    IDirectInputDevice8W* device,
    DWORD cb_data,
    LPVOID lpv_data) {
    auto& self = Dx11Hook::Instance();
    if (!self.shutting_down_.load() &&
        (GetTickCount64() - self.last_device_change_tick_.load() >= 2000ULL)) {
        const bool is_mouse_format = (cb_data == sizeof(DIMOUSESTATE) || cb_data == sizeof(DIMOUSESTATE2));
        if (is_mouse_format) {
            self.active_mouse_device_.store(device);
        } else {
            // Keyboard (256) or Joystick/Wheel/Gamepad (80 / 272): cache as non-mouse without calling GetCapabilities
            MarkNonMouseDevice(device);
        }

        if (is_mouse_format) {
            if (nismara::ui::TabletGui::Instance().IsVisible()) {
                if (!self.mouse_unacquired_for_overlay_.exchange(true) && self.original_di_unacquire_ != nullptr) {
                    SafeCallDIUnacquire(self.original_di_unacquire_, device);
                }
                if (lpv_data != nullptr && cb_data > 0) {
                    std::memset(lpv_data, 0, cb_data);
                }
                return DI_OK;
            } else if (self.mouse_unacquired_for_overlay_.exchange(false)) {
                if (self.original_di_acquire_ != nullptr) {
                    SafeCallDIAcquire(self.original_di_acquire_, device);
                }
            }
        }
    }

    if (self.original_di_get_device_state_ != nullptr) {
        return SafeCallDIGetDeviceState(self.original_di_get_device_state_, device, cb_data, lpv_data);
    }
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE Dx11Hook::HookedDIGetDeviceData(
    IDirectInputDevice8W* device,
    DWORD cb_object_data,
    LPDIDEVICEOBJECTDATA rgdod,
    LPDWORD pdw_in_out,
    DWORD dw_flags) {
    auto& self = Dx11Hook::Instance();
    const bool overlay_visible = nismara::ui::TabletGui::Instance().IsVisible();
    const bool needs_reacquire = self.mouse_unacquired_for_overlay_.load();

    // Fast path: when phone is closed and mouse doesn't need re-acquire, pass straight through
    // without touching any COM methods on joysticks/wheels/gamepads!
    if (!self.shutting_down_.load() &&
        (overlay_visible || needs_reacquire) &&
        (GetTickCount64() - self.last_device_change_tick_.load() >= 2000ULL)) {
        const bool is_mouse = IsMouseDevice(device);

        if (overlay_visible) {
            if (is_mouse) {
                if (!self.mouse_unacquired_for_overlay_.exchange(true) && self.original_di_unacquire_ != nullptr) {
                    SafeCallDIUnacquire(self.original_di_unacquire_, device);
                }
                if (pdw_in_out != nullptr) {
                    *pdw_in_out = 0;
                }
                return DI_OK;
            }
        } else if (is_mouse && self.mouse_unacquired_for_overlay_.exchange(false)) {
            if (self.original_di_acquire_ != nullptr) {
                SafeCallDIAcquire(self.original_di_acquire_, device);
            }
        }
    }

    if (self.original_di_get_device_data_ != nullptr) {
        return SafeCallDIGetDeviceData(self.original_di_get_device_data_, device, cb_object_data, rgdod, pdw_in_out, dw_flags);
    }
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE Dx11Hook::HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    auto& self = Dx11Hook::Instance();

    if (self.shutting_down_.load() || self.original_present_ == nullptr) {
        return (self.original_present_ != nullptr)
            ? SafeCallPresent(self.original_present_, swap_chain, sync_interval, flags)
            : S_OK;
    }

    // 1. If this is a test query, do not render or touch swap chain buffers
    if (flags & DXGI_PRESENT_TEST) {
        return SafeCallPresent(self.original_present_, swap_chain, sync_interval, flags);
    }

    // 2. If the window is minimized or not a valid window, do not render overlays
    if (self.game_hwnd_ != nullptr && (IsIconic(self.game_hwnd_) || !IsWindow(self.game_hwnd_))) {
        return SafeCallPresent(self.original_present_, swap_chain, sync_interval, flags);
    }

    try {
        if (!self.imgui_initialized_.load()) {
            self.InitializeImGui(swap_chain);
        }

        if (self.imgui_initialized_.load() && self.context_ != nullptr) {
            // Safety Guard: Strictly restrict phone overlay to active truck cabin driving sessions.
            // Disallowed in Main Menu, Loading Screens, or when Paused.
            const bool can_render = nismara::core::TelemetryState::Instance().IsInCabinDrivingSession();
            if (!can_render && nismara::ui::TabletGui::Instance().IsVisible()) {
                nismara::ui::TabletGui::Instance().SetVisible(false);
            }

            const bool tablet_visible = nismara::ui::TabletGui::Instance().IsVisible() && can_render;
            if (tablet_visible != self.last_frame_visible_) {
                self.last_frame_visible_ = tablet_visible;
                self.OnOverlayVisibilityChanged(tablet_visible);
                if (!tablet_visible) {
                    // Immediately release swapchain backbuffer reference when overlay is closed
                    self.CleanupRenderTarget();
                }
            }

            if (tablet_visible) {
                if (self.main_rtv_ == nullptr) {
                    self.CreateRenderTarget(swap_chain);
                }

                if (self.main_rtv_ != nullptr) {
                    // Initialize and update Ultralight web surface bridge on-demand
                    auto& web_bridge = nismara::ui::WebSurfaceBridge::Instance();
                    if (!web_bridge.IsInitialized() && !self.web_bridge_init_attempted_) {
                        self.web_bridge_init_attempted_ = true;
                        web_bridge.Initialize();
                    }
                    if (web_bridge.IsInitialized()) {
                        web_bridge.DispatchTelemetry();
                        web_bridge.UpdateTexture(self.device_, self.context_);
                    }

                    ImGui_ImplDX11_NewFrame();
                    ImGui_ImplWin32_NewFrame();
                    ImGui::NewFrame();

                    ImGui::GetIO().MouseDrawCursor = true;
                    nismara::ui::TabletGui::Instance().Render();

                    ImGui::Render();
                    self.context_->OMSetRenderTargets(1, &self.main_rtv_, nullptr);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                }
            }
        }
    } catch (...) {
        // Overlay rendering error must never take down the swap chain
    }

    return SafeCallPresent(self.original_present_, swap_chain, sync_interval, flags);
}

HRESULT STDMETHODCALLTYPE Dx11Hook::HookedResizeBuffers(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT new_format,
    UINT swap_chain_flags) {
    auto& self = Dx11Hook::Instance();
    try {
        nismara::ui::WebSurfaceBridge::Instance().ReleaseD3DResources();
        self.CleanupRenderTarget();
    } catch (...) {}

    if (self.original_resize_buffers_ != nullptr) {
        return self.original_resize_buffers_(
            swap_chain,
            buffer_count,
            width,
            height,
            new_format,
            swap_chain_flags);
    }
    return S_OK;
}

} // namespace nismara::hooks
