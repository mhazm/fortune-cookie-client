cmake_minimum_required(VERSION 3.20)

# Enable MSVC_RUNTIME_LIBRARY target property support (CMP0091)
cmake_policy(SET CMP0091 NEW)

project(NismaraTracker VERSION 1.0.1 LANGUAGES C CXX)

# Enforce 64-bit target architecture
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "NismaraTracker must be compiled as a 64-bit (x64) target for ETS2/ATS.")
endif()

# C++20 Standard Configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static Runtime Linking (/MT in Release, /MTd in Debug) - Zero external VC++ Redistributable dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Vendor Sources: MinHook (x64)
set(MINHOOK_SOURCES
    vendor/minhook/src/buffer.c
    vendor/minhook/src/hook.c
    vendor/minhook/src/trampoline.c
    vendor/minhook/src/hde/hde64.c
)

# Vendor Sources: Dear ImGui (Docking Branch + DirectX 11 & Win32 Backends)
set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_dx11.cpp
    vendor/imgui/backends/imgui_impl_win32.cpp
)

# Suppress third-party warnings on vendor files so our own code compiles cleanly under /W4
set_source_files_properties(${MINHOOK_SOURCES} ${IMGUI_SOURCES} PROPERTIES COMPILE_FLAGS "/W0")

# NismaraTracker Plugin Sources
set(NISMARA_SOURCES
    src/dllmain.cpp
    src/NismaraTracker.def
    src/core/telemetry_state.cpp
    src/core/config_manager.cpp
    src/core/api_client.cpp
    src/core/server_detector.cpp
    src/core/profile_validator.cpp
    src/core/scs_callbacks.cpp
    src/hooks/dx11_hook.cpp
    src/hooks/wndproc_hook.cpp
    src/ui/texture_manager.cpp
    src/ui/tablet_gui.cpp
    src/ui/web_surface_bridge.cpp
)

add_library(NismaraTracker SHARED
    ${NISMARA_SOURCES}
    ${MINHOOK_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(NismaraTracker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/scs-sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/minhook/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ultralight/include
)

target_link_directories(NismaraTracker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ultralight/lib
)

target_compile_definitions(NismaraTracker PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    NISMARA_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(NismaraTracker PRIVATE
        /W4
        /MP
        /EHsc
        /utf-8
    )
    target_link_options(NismaraTracker PRIVATE
        "/DELAYLOAD:Ultralight.dll"
        "/DELAYLOAD:UltralightCore.dll"
        "/DELAYLOAD:WebCore.dll"
        "/DELAYLOAD:AppCore.dll"
    )
endif()

target_link_libraries(NismaraTracker PRIVATE
    d3d11.lib
    dxgi.lib
    dxguid.lib
    user32.lib
    gdi32.lib
    winhttp.lib
    windowscodecs.lib
    ole32.lib
    iphlpapi.lib
    ws2_32.lib
    shell32.lib
    delayimp.lib
    Ultralight.lib
    UltralightCore.lib
    WebCore.lib
    AppCore.lib
)

# Output NismaraTracker.dll directly into ${CMAKE_SOURCE_DIR}/bin/ across all configurations
set_target_properties(NismaraTracker PROPERTIES
    OUTPUT_NAME "NismaraTracker"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
