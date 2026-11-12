#include <windows.h>
#include <filesystem>

#include "core/api_client.hpp"
#include "core/config_manager.hpp"
#include "hooks/dx11_hook.hpp"

namespace {

LONG WINAPI NismaraCrashHandler(EXCEPTION_POINTERS* /*pExceptionInfo*/) {
    // Attempt emergency synchronous checkpoint dispatch before Windows terminates the process
    nismara::core::ApiClient::Instance().EmergencyFlushCheckpoint();
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            SetUnhandledExceptionFilter(NismaraCrashHandler);

            // Register Companion Ultralight runtime directory into Windows DLL search path
            // so MSVC Delay-Load resolves Ultralight.dll, WebCore.dll, etc. directly from %LOCALAPPDATA%
            const auto runtime_dir = nismara::core::ConfigManager::Instance().GetCompanionRuntimeDirectory();
            std::error_code ec;
            if (!runtime_dir.empty() && std::filesystem::exists(runtime_dir, ec)) {
                SetDllDirectoryW(runtime_dir.c_str());
            }
            break;
        }

        case DLL_PROCESS_DETACH:
            if (lpReserved != nullptr) {
                // Process is terminating! OS terminates all background worker threads.
                // Never wait/join threads or invoke COM/DirectX during process exit under loader lock.
                return TRUE;
            }
            nismara::core::ApiClient::Instance().Shutdown();
            nismara::hooks::Dx11Hook::Instance().Shutdown();
            break;

        default:
            break;
    }
    return TRUE;
}
