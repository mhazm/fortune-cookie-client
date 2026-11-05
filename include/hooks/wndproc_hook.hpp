#pragma once

#include <windows.h>
#include <atomic>

namespace nismara::hooks {

/**
 * @brief Intercepts the game's Win32 WndProc via SetWindowLongPtr to handle the
 *        F11 overlay toggle hotkey and route mouse/keyboard focus to Dear ImGui.
 */
class WndProcHook {
public:
    static WndProcHook& Instance();

    WndProcHook(const WndProcHook&) = delete;
    WndProcHook& operator=(const WndProcHook&) = delete;

    /**
     * @brief Attaches the WndProc hook to the target game window handle.
     */
    bool Install(HWND hwnd);

    /**
     * @brief Restores the original WndProc to the game window handle.
     */
    void Shutdown();

    /**
     * @brief Returns true if the WndProc hook is currently installed.
     */
    bool IsInstalled() const noexcept { return installed_.load(); }

private:
    WndProcHook() = default;
    ~WndProcHook() = default;

    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_{nullptr};
    WNDPROC original_wndproc_{nullptr};
    bool is_unicode_{true};
    std::atomic<bool> installed_{false};
};

} // namespace nismara::hooks

