# UltralightConfig.cmake -- CMake package definition for the Ultralight SDK.
#
#   find_package(Ultralight REQUIRED)
#   add_executable(my_app main.cpp)
#   target_link_libraries(my_app PRIVATE Ultralight::AppCore)
#   ultralight_copy_runtime_files(my_app)
#
# Point CMake at the SDK with -DCMAKE_PREFIX_PATH=<sdk-root>, or with
# -DUltralight_DIR=<sdk-root>/lib/cmake/Ultralight.
#
# Imported targets:
#
#   Ultralight::UltralightCore  Graphics primitives.
#   Ultralight::WebCore         HTML, CSS, and JavaScript engine.
#   Ultralight::Ultralight      Renderer, View, and Session. Brings in the two
#                               targets above.
#   Ultralight::AppCore         App, Window, and the GPU drivers. Brings in all
#                               of the above.
#
# Variables:
#
#   ULTRALIGHT_VERSION        Library version string.
#   ULTRALIGHT_INCLUDE_DIR    Public headers.
#   ULTRALIGHT_BINARY_DIR     Shared libraries.
#   ULTRALIGHT_LIBRARY_DIR    Link libraries.
#   ULTRALIGHT_RESOURCES_DIR  Data files the renderer loads at runtime.
#   ULTRALIGHT_INSPECTOR_DIR  Web Inspector front end.
#   ULTRALIGHT_PLATFORM_DIR   Platform reference sources. Empty when this
#                             package does not include them.
#   ULTRALIGHT_STATIC_BUILD   TRUE when this package links statically.


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was UltralightConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set(ULTRALIGHT_VERSION "2.0.0")
set(ULTRALIGHT_STATIC_BUILD OFF)

set(_ultralight_postfix "")

set_and_check(ULTRALIGHT_INCLUDE_DIR   "${PACKAGE_PREFIX_DIR}/include")
set_and_check(ULTRALIGHT_BINARY_DIR    "${PACKAGE_PREFIX_DIR}/bin")
set_and_check(ULTRALIGHT_RESOURCES_DIR "${PACKAGE_PREFIX_DIR}/resources")

set_and_check(ULTRALIGHT_LIBRARY_DIR "${PACKAGE_PREFIX_DIR}/lib")

# Optional in a package, so no existence check.
set(ULTRALIGHT_INSPECTOR_DIR "${PACKAGE_PREFIX_DIR}/inspector")

# The Platform reference implementations ship as source, not as a binary. Build
# them with add_subdirectory(${ULTRALIGHT_PLATFORM_DIR} platform) and link
# Ultralight::Platform when the app drives the reference GPU drivers, contexts,
# or services directly instead of going through App::Create.
set(ULTRALIGHT_PLATFORM_DIR "${PACKAGE_PREFIX_DIR}/platform")
if (NOT EXISTS "${ULTRALIGHT_PLATFORM_DIR}/CMakeLists.txt")
    set(ULTRALIGHT_PLATFORM_DIR "")
endif ()

function(_ultralight_add_imported_library name)
    if (TARGET Ultralight::${name})
        return()
    endif ()

    set(base "${name}${_ultralight_postfix}")

    if (ULTRALIGHT_STATIC_BUILD)
        add_library(Ultralight::${name} STATIC IMPORTED)
        set_target_properties(Ultralight::${name} PROPERTIES IMPORTED_LOCATION
            "${ULTRALIGHT_LIBRARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${base}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    elseif (WIN32)
        # The libraries are in bin/ and their import libraries in lib/.
        add_library(Ultralight::${name} SHARED IMPORTED)
        set_target_properties(Ultralight::${name} PROPERTIES
            IMPORTED_LOCATION "${ULTRALIGHT_BINARY_DIR}/${base}.dll"
            IMPORTED_IMPLIB   "${ULTRALIGHT_LIBRARY_DIR}/${base}.lib")
    else ()
        add_library(Ultralight::${name} SHARED IMPORTED)
        set_target_properties(Ultralight::${name} PROPERTIES IMPORTED_LOCATION
            "${ULTRALIGHT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${base}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif ()

    # The public headers refuse to compile below C++20.
    set_target_properties(Ultralight::${name} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ULTRALIGHT_INCLUDE_DIR}"
        INTERFACE_COMPILE_FEATURES cxx_std_20)
endfunction ()

foreach (_ultralight_lib IN ITEMS UltralightCore WebCore Ultralight AppCore)
    _ultralight_add_imported_library(${_ultralight_lib})
endforeach ()

# Linking the highest layer an app uses brings in the layers below it.
set_property(TARGET Ultralight::Ultralight APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES Ultralight::UltralightCore Ultralight::WebCore)
set_property(TARGET Ultralight::AppCore APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES Ultralight::Ultralight)

if (ULTRALIGHT_STATIC_BUILD)
    foreach (_ultralight_lib IN ITEMS UltralightCore WebCore Ultralight AppCore)
        set_property(TARGET Ultralight::${_ultralight_lib} APPEND PROPERTY
            INTERFACE_COMPILE_DEFINITIONS ULTRALIGHT_STATIC_BUILD)
    endforeach ()

    # A shared package folds these into its libraries; a static one links them
    # alongside. The engine archives take the platform's archive prefix; the
    # third-party archives are named as built.
    set(_ultralight_engine_archives "")
    set(_ultralight_deps_archives "")
    foreach (_ultralight_archive IN LISTS _ultralight_engine_archives)
        set_property(TARGET Ultralight::WebCore APPEND PROPERTY INTERFACE_LINK_LIBRARIES
            "${ULTRALIGHT_LIBRARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${_ultralight_archive}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endforeach ()
    foreach (_ultralight_archive IN LISTS _ultralight_deps_archives)
        set_property(TARGET Ultralight::WebCore APPEND PROPERTY INTERFACE_LINK_LIBRARIES
            "${ULTRALIGHT_LIBRARY_DIR}/${_ultralight_archive}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endforeach ()

    set_property(TARGET Ultralight::WebCore APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "")
    set_property(TARGET Ultralight::AppCore APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "")
endif ()

##
## Stage what the renderer needs at runtime next to the target's executable:
## the shared libraries, and the resources directory under assets/ where the
## default file system looks for it. Call this after add_executable().
##
function(ultralight_copy_runtime_files target)
    if (NOT ULTRALIGHT_STATIC_BUILD)
        # The executable has to look next to itself for the copies: Windows does
        # so by default, macOS and Linux only through an rpath.
        if (APPLE)
            set_target_properties(${target} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "@executable_path/")
        elseif (UNIX)
            set_target_properties(${target} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN")
        endif ()
        foreach (lib IN ITEMS UltralightCore WebCore Ultralight AppCore)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Ultralight::${lib}>" "$<TARGET_FILE_DIR:${target}>")
        endforeach ()
    endif ()

    # A macOS bundle resolves file:/// against Contents/Resources/, not the
    # folder that holds the executable (Contents/MacOS/).
    set(assets_dir "$<TARGET_FILE_DIR:${target}>/assets")
    if (APPLE)
        get_target_property(is_bundle ${target} MACOSX_BUNDLE)
        if (is_bundle)
            set(assets_dir "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/assets")
        endif ()
    endif ()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ULTRALIGHT_RESOURCES_DIR}" "${assets_dir}/resources")
endfunction ()

check_required_components(Ultralight)
