# MISSION SPECIFICATION: ETS2/ATS IN-GAME TELEMETRY & OVERLAY PLUGIN (PHASE 1)

## 1. PROJECT OBJECTIVE
Build a high-performance, single-binary C++ client plugin for Euro Truck Simulator 2 and American Truck Simulator using the SCS Telemetry SDK. The project code name is `NismaraTracker`.

The final output artifact must be a single standalone dynamic link library (`NismaraTracker.dll`, x64) placed in `bin/win_x64/plugins/` that works without any additional `.scs` mod files. It must function seamlessly across Vanilla, SCS Convoy, TruckersMP, and DihaulMP.

---

## 2. ARCHITECTURAL REQUIREMENTS & CONSTRAINTS

| Parameter | Specification |
| :--- | :--- |
| **Language & Standard** | C++20 (MSVC x64) |
| **Runtime Linking** | Multi-threaded Statically Linked (`/MT` in Release, `/MTd` in Debug). Zero external Visual C++ Redistributable requirement on target user machines. |
| **DirectX Hooking** | MinHook (statically compiled) targeting `IDXGISwapChain::Present` on DirectX 11. |
| **Input Interception** | Win32 API `WndProc` hook via `SetWindowLongPtr` for mouse/keyboard focus capture. |
| **UI Framework** | Dear ImGui (Docking branch) with DirectX 11 + Win32 backend. |
| **Telemetry Interface**| SCS Telemetry SDK v1.10+ C-API (`scs_telemetry_init`, `scs_telemetry_shutdown`). |
| **Thread Model** | Strict thread-safety using `std::shared_mutex` to isolate the game rendering loop, SDK callbacks, and future network workers. |
| **Target OS** | Windows 10 / Windows 11 (64-bit). |

---

## 3. TARGET DIRECTORY STRUCTURE

Scaffold the project with the following modular layout:

```text
nismara-tracker/
├── CMakeLists.txt
├── .gitignore
├── include/
│   ├── scs/
│   │   ├── scs-telemetry-common.h
│   │   └── scs-telemetry.h
│   ├── core/
│   │   ├── telemetry_state.hpp
│   │   └── config_manager.hpp
│   ├── hooks/
│   │   ├── dx11_hook.hpp
│   │   └── wndproc_hook.hpp
│   └── ui/
│       └── tablet_gui.hpp
├── src/
│   ├── dllmain.cpp
│   ├── core/
│   │   ├── telemetry_state.cpp
│   │   ├── config_manager.cpp
│   │   └── scs_callbacks.cpp
│   ├── hooks/
│   │   ├── dx11_hook.cpp
│   │   └── wndproc_hook.cpp
│   └── ui/
│       └── tablet_gui.cpp
└── vendor/
    ├── minhook/
    └── imgui/
```

---

## 4. PHASE 1 IMPLEMENTATION TASKS

### Task 1: Build System Configuration (`CMakeLists.txt`)
- Configure CMake targeting MSVC 64-bit shared library (`SHARED`).
- Set compiler flags: `/std:c++20 /W4 /MT`.
- Link standard system libraries: `d3d11.lib`, `dxgi.lib`, `user32.lib`, `gdi32.lib`.
- Include and compile vendor source files statically (Dear ImGui core files, DX11/Win32 backends, MinHook).
- Set binary output directory to `${CMAKE_SOURCE_DIR}/bin/NismaraTracker.dll`.

### Task 2: Core SDK Hooks & Thread-Safe State
1. In `telemetry_state.hpp`, define `TelemetrySnapshot`:
   - Game identifier (ETS2 / ATS) and engine version.
   - Truck metrics: engine running, current gear, speed (km/h), engine RPM, fuel current and capacity.
   - Vector coordinates: world position `(x, y, z)` and orientation `heading`.
   - Structural health: cabin wear, chassis wear, wheels wear, trailer wear.
   - Active job contract: cargo name, mass (kg), origin city/company, destination city/company, contract deadline, reward payout.
2. In `scs_callbacks.cpp`, establish SDK binding:
   - Implement `scs_telemetry_init` and `scs_telemetry_shutdown`.
   - Register periodic channels for mechanical data and position.
   - Register event callbacks for `SCS_TELEMETRY_EVENT_configuration` and `SCS_TELEMETRY_EVENT_gameplay` (`job_started`, `job_finished`, `job_cancelled`).
3. Protect writes from SDK callbacks with `std::unique_lock<std::shared_mutex>`, and reads from rendering threads with `std::shared_lock<std::shared_mutex>`.

### Task 3: DirectX 11 Hooking & Detour Pipeline
1. In `dx11_hook.cpp`, create a dummy DirectX 11 device and swap chain to extract the VTable address of `IDXGISwapChain::Present`.
2. Apply MinHook detour on `Present`:
   - Enforce an initial worker thread delay of 3–5 seconds before attaching the hook to prevent race conditions with multiplayer overlays (TruckersMP / DihaulMP).
   - On the first valid frame, initialize Dear ImGui context, link Win32 window handles, and create the `ID3D11RenderTargetView`.
   - Execute `tablet_gui::Render()` before delegating execution to the original `Present` address.
3. In `wndproc_hook.cpp`, hook window message events:
   - Bind hotkey toggle (default: `F11`).
   - When the overlay is toggled visible: route mouse/keyboard input to ImGui and suppress camera movement in-game.
   - When hidden: forward raw input back to the simulation pipeline.

### Task 4: In-Game Tablet Interface Prototype
1. In `tablet_gui.cpp`, render a floating Dear ImGui window styled as a modern vehicle tablet:
   - Title: `NISMARA OS v1.0 [SIMULATION UNIT]`.
   - Visual styling: Dark theme, rounded window corners, responsive tab navigation.
   - **Tab: Dashboard:** Live speedometer, RPM gauge, current gear, fuel status, cumulative damage indicators, and real-time coordinates.
   - **Tab: Job Status:** Active cargo info, source, destination, and calculated progress.
   - **Tab: Driver Account:** Input text box for driver authentication token and a "Save Config" trigger.
2. In `config_manager.cpp`, persist configuration:
   - Save driver token and UI preferences to a local configuration file (`nismara_config.dat`) in the plugins folder so credentials survive game restarts.

---

## 5. ACCEPTANCE CRITERIA
1. The project compiles cleanly on MSVC without missing external dependencies, generating a single `NismaraTracker.dll` binary (< 15 MB).
2. Placing the DLL in `Euro Truck Simulator 2/bin/win_x64/plugins/` boots with the game without crashing during initialization or profile loading.
3. Pressing `F11` opens the tablet overlay, allowing interactive mouse control.
4. Telemetry values (speed, RPM, coordinates) accurately mirror truck movement in real-time.
5. The DLL shuts down cleanly when exiting the game, restoring original hooks without segmentation faults.
```