#include <windows.h>

#include "core/api_client.hpp"
#include "core/config_manager.hpp"
#include "hooks/dx11_hook.hpp"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            break;

        case DLL_PROCESS_DETACH:
            (void)lpReserved;
            nismara::core::ApiClient::Instance().Shutdown();
            nismara::hooks::Dx11Hook::Instance().Shutdown();
            break;

        default:
            break;
    }
    return TRUE;
}
