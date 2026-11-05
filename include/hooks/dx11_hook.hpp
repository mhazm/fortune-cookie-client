#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <atomic>
#include <thread>

namespace nismara::hooks {

/**
 * @brief Manages DirectX 11 IDXGISwapChain::Present and ResizeBuffers hooking,
 *        Win32 cursor lock interception (SetCursorPos / ClipCursor), and
 *        DirectInput8 mouse/keyboard interception via MinHook.
 */
class Dx11Hook {
public:
    static Dx11Hook& Instance();

    Dx11Hook(const Dx11Hook&) = delete;
    Dx11Hook& operator=(const Dx11Hook&) = delete;

    /**
     * @brief Spawns a background worker thread that waits 4 seconds before attaching
     *        the DirectX 11, Win32 cursor, and DirectInput8 hooks.
     */
    void StartAsync();

    /**
     * @brief Cleanly unhooks all detours, restores WndProc, and frees DX11/ImGui resources.
     */
    void Shutdown();

    /**
     * @brief Called when the tablet visibility state changes to immediately unlock/relock cursor.
     */
    void OnOverlayVisibilityChanged(bool visible);

    ID3D11Device* GetDevice() const noexcept { return device_; }
    ID3D11DeviceContext* GetContext() const noexcept { return context_; }

private:
    Dx11Hook() = default;
    ~Dx11Hook() = default;

    void WorkerThreadProc();
    bool ExtractSwapChainVTable(void** out_present, void** out_resize_buffers);
    bool ExtractDirectInputVTable(
        void** out_acquire,
        void** out_unacquire,
        void** out_get_device_state,
        void** out_get_device_data);

    void InitializeImGui(IDXGISwapChain* swap_chain);
    void CreateRenderTarget(IDXGISwapChain* swap_chain);
    void CleanupRenderTarget();

    static bool IsMouseDevice(IDirectInputDevice8W* device);

    // DirectX 11 Detours
    static HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
    static HRESULT STDMETHODCALLTYPE HookedResizeBuffers(
        IDXGISwapChain* swap_chain,
        UINT buffer_count,
        UINT width,
        UINT height,
        DXGI_FORMAT new_format,
        UINT swap_chain_flags);

    // Win32 Cursor Detours (prevents ETS2/ATS from snapping cursor back to screen center)
    static BOOL WINAPI HookedSetCursorPos(int x, int y);
    static BOOL WINAPI HookedClipCursor(const RECT* rect);

    // DirectInput8 Detours (releases exclusive mouse lock & suppresses camera rotation while tablet is open)
    static HRESULT STDMETHODCALLTYPE HookedDIAcquire(IDirectInputDevice8W* device);
    static HRESULT STDMETHODCALLTYPE HookedDIGetDeviceState(IDirectInputDevice8W* device, DWORD cb_data, LPVOID lpv_data);
    static HRESULT STDMETHODCALLTYPE HookedDIGetDeviceData(
        IDirectInputDevice8W* device,
        DWORD cb_object_data,
        LPDIDEVICEOBJECTDATA rgdod,
        LPDWORD pdw_in_out,
        DWORD dw_flags);

    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using SetCursorPosFn = BOOL(WINAPI*)(int, int);
    using ClipCursorFn = BOOL(WINAPI*)(const RECT*);
    using DIAcquireFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*);
    using DIUnacquireFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*);
    using DIGetDeviceStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPVOID);
    using DIGetDeviceDataFn =
        HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

    PresentFn original_present_{nullptr};
    ResizeBuffersFn original_resize_buffers_{nullptr};
    SetCursorPosFn original_set_cursor_pos_{nullptr};
    ClipCursorFn original_clip_cursor_{nullptr};
    DIAcquireFn original_di_acquire_{nullptr};
    DIUnacquireFn original_di_unacquire_{nullptr};
    DIGetDeviceStateFn original_di_get_device_state_{nullptr};
    DIGetDeviceDataFn original_di_get_device_data_{nullptr};

    ID3D11Device* device_{nullptr};
    ID3D11DeviceContext* context_{nullptr};
    ID3D11RenderTargetView* main_rtv_{nullptr};
    HWND game_hwnd_{nullptr};

    std::atomic<IDirectInputDevice8W*> active_mouse_device_{nullptr};
    std::atomic<bool> mouse_unacquired_for_overlay_{false};
    POINT saved_overlay_cursor_pos_{-1, -1};
    bool last_frame_visible_{false};

    std::thread worker_thread_{};
    std::atomic<bool> started_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> hook_installed_{false};
    std::atomic<bool> imgui_initialized_{false};
    std::atomic<bool> shutting_down_{false};
};

} // namespace nismara::hooks
