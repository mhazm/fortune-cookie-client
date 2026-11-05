#pragma once

#include <windows.h>

namespace nismara::core {

/**
 * @brief Checks if Nismara Companion Desktop App is currently running in the background.
 *        The desktop app holds a Win32 Named Mutex (Global\NismaraCompanionRunning
 *        or Local\NismaraCompanionRunning) while active.
 */
inline bool IsCompanionAppRunning() {
    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Global\\NismaraCompanionRunning");
    if (hMutex == nullptr) {
        hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\NismaraCompanionRunning");
    }
    if (hMutex != nullptr) {
        CloseHandle(hMutex);
        return true;
    }
    return false;
}

} // namespace nismara::core

