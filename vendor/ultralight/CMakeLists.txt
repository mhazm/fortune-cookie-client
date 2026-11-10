cmake_minimum_required(VERSION 3.15.0)

project(Ultralight-SDK
        LANGUAGES C CXX
        VERSION 2.0.0)

# Makefile and Ninja builds compile without optimization when no build type is
# given.
get_property(UL_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT UL_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

get_filename_component(UL_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)

list(APPEND CMAKE_MODULE_PATH ${UL_SDK_PATH}/cmake)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/out" CACHE PATH "install prefix" FORCE)

# The Platform reference implementations (GPU drivers, contexts, swap chains,
# font loaders, file systems, ...) build as the Ultralight::Platform static
# library for apps that drive them directly instead of through App::Create.
# Not every SDK package includes the directory.
if (EXISTS "${UL_SDK_PATH}/platform/CMakeLists.txt")
    add_subdirectory(platform)
endif ()

add_subdirectory(samples)
add_subdirectory(tools)