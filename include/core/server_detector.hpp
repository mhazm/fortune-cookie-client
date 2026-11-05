#pragma once

#include <atomic>
#include <thread>

namespace nismara::core {

/**
 * @brief Multi-layer in-process server & multiplayer environment detector for ETS2/ATS.
 *
 * Since SCS Telemetry SDK v1.01/1.14 does not natively expose a multiplayer server channel,
 * ServerDetector inspects 4 live signals inside the eurotrucks2.exe / amtrucks.exe process:
 *  1. Loaded Process Modules (DLLs injected by TruckersMP `core_ets2mp.dll`/`core_atsmp.dll`,
 *     HaulMP `haulmp*.dll`, or Steam Networking `steamnetworkingsockets.dll`).
 *  2. Process Command Line & Main Window Title (`GetCommandLineA`).
 *  3. Game Log & MP Client Log Tailing (`Documents/Euro Truck Simulator 2/game.log.txt` for
 *     SCS `[mp]` Convoy session events & `Documents/ETS2MP/logs/client_*.log` for TruckersMP server names).
 *  4. Active Process Network Endpoints (`GetExtendedTcpTable` / `GetExtendedUdpTable` for PID).
 */
class ServerDetector {
public:
    static ServerDetector& Instance();

    ServerDetector(const ServerDetector&) = delete;
    ServerDetector& operator=(const ServerDetector&) = delete;

    /**
     * @brief Starts the background periodic detection worker thread.
     */
    void Initialize();

    /**
     * @brief Stops the background worker thread cleanly on plugin shutdown.
     */
    void Shutdown();

    /**
     * @brief Triggers an immediate synchronous scan on a detached/background check or directly.
     */
    void RequestImmediateScan();

private:
    ServerDetector() = default;
    ~ServerDetector() = default;

    void WorkerLoop();
    void PerformScan();

    std::atomic<bool> running_{false};
    std::atomic<bool> force_scan_{false};
    std::thread worker_thread_;
};

} // namespace nismara::core

