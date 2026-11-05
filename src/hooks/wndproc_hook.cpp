#include "hooks/wndproc_hook.hpp"
#include "core/api_client.hpp"
#include "core/companion_gate.hpp"
#include "core/config_manager.hpp"
#include "core/telemetry_state.hpp"
#include "hooks/dx11_hook.hpp"
#include "ui/tablet_gui.hpp"

#include "imgui.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
LRESULT SafeCallWindowProc(WNDPROC proc, bool is_unicode, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (proc == nullptr) {
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    __try {
        return is_unicode ? CallWindowProcW(proc, hWnd, uMsg, wParam, lParam)
                          : CallWindowProcA(proc, hWnd, uMsg, wParam, lParam);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}
} // namespace

namespace nismara::hooks {

WndProcHook& WndProcHook::Instance() {
    static WndProcHook instance;
    return instance;
}

bool WndProcHook::Install(HWND hwnd) {
    if (installed_.load() || hwnd == nullptr || !IsWindow(hwnd)) {
        return false;
    }

    hwnd_ = hwnd;
    is_unicode_ = (IsWindowUnicode(hwnd_) != FALSE);

    if (is_unicode_) {
        original_wndproc_ = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProcHook::HookedWndProc)));
    } else {
        original_wndproc_ = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProcHook::HookedWndProc)));
    }

    if (original_wndproc_ == nullptr) {
        hwnd_ = nullptr;
        return false;
    }

    installed_.store(true);
    return true;
}

void WndProcHook::Shutdown() {
    if (!installed_.exchange(false)) {
        return;
    }

    if (hwnd_ != nullptr && original_wndproc_ != nullptr && IsWindow(hwnd_)) {
        if (is_unicode_) {
            SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc_));
        } else {
            SetWindowLongPtrA(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc_));
        }
    }

    original_wndproc_ = nullptr;
    hwnd_ = nullptr;
}

LRESULT CALLBACK WndProcHook::HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto& self = WndProcHook::Instance();

    // Check toggle hotkey (Default: VK_F11) on key-down without auto-repeat
    if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
        const int hotkey_vk = nismara::core::ConfigManager::Instance().GetConfig().toggle_hotkey_vk;
        const bool is_repeat = (lParam & (1LL << 30)) != 0;
        if (static_cast<int>(wParam) == hotkey_vk && !is_repeat) {
            // Gatekeeper: Only toggle phone if Nismara Companion App is running in the background!
            // If Companion is not active, pressing F11 does nothing.
            if (!nismara::core::IsCompanionAppRunning()) {
                if (nismara::ui::TabletGui::Instance().IsVisible()) {
                    nismara::ui::TabletGui::Instance().SetVisible(false);
                    Dx11Hook::Instance().OnOverlayVisibilityChanged(false);
                }
                return 0;
            }

            nismara::ui::TabletGui::Instance().ToggleVisible();
            const bool visible = nismara::ui::TabletGui::Instance().IsVisible();
            Dx11Hook::Instance().OnOverlayVisibilityChanged(visible);

            if (visible) {
                const auto acc = nismara::core::TelemetryState::Instance().GetDriverAccount();
                if (acc.status == nismara::core::AccountSyncStatus::Disconnected &&
                    !nismara::core::ConfigManager::Instance().GetConfig().driver_token.empty()) {
                    nismara::core::ApiClient::Instance().SyncAccountAsync();
                }
            }
            return 0;
        }
    }

    // When tablet overlay is visible, route inputs to ImGui and block game camera/controls
    if (nismara::ui::TabletGui::Instance().IsVisible() && ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

        switch (uMsg) {
            // Suppress Raw Input (used by ETS2/ATS for cabin camera rotation)
            case WM_INPUT:
                return 0;

            // Suppress Win32 mouse messages from reaching the simulation
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_SETCURSOR:
                return 1;

            // Suppress keyboard messages when typing in ImGui inputs (e.g. Driver Token)
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
                if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput) {
                    return 0;
                }
                break;

            default:
                break;
        }
    }

    if (uMsg == WM_NCDESTROY) {
        WNDPROC orig = self.original_wndproc_;
        const bool is_unicode = self.is_unicode_;
        self.Shutdown();
        return SafeCallWindowProc(orig, is_unicode, hWnd, uMsg, wParam, lParam);
    }

    if (self.original_wndproc_ != nullptr) {
        return SafeCallWindowProc(self.original_wndproc_, self.is_unicode_, hWnd, uMsg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

} // namespace nismara::hooks

