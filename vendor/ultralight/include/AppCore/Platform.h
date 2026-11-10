///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file Platform.h
///
/// Stock platform handlers for applications that manage their own renderer.
///
/// `#include <AppCore/Platform.h>`
///
/// The library delegates OS tasks to the Platform singleton. When you manage your own event loop
/// and create a Renderer directly, nothing fills the platform handlers for you. This header
/// provides stock implementations so you don't have to write custom handlers yourself.
///
/// This example registers the stock handlers on the platform singleton before creating the
/// renderer:
///
/// ```
/// RefPtr<Renderer> CreateRenderer() {
///   Platform::instance().set_font_loader(GetPlatformFontLoader());
///   Platform::instance().set_file_system(GetPlatformFileSystem("./assets/"));
///   Platform::instance().set_logger(GetDefaultLogger("./ultralight.log"));
///
///   return Renderer::Create();
/// }
/// ```
///
/// ## Handler Requirements
///
/// Whether you need to call these functions depends on how you initialize the library:
///
/// - **Applications using App::Create() don't need this header.** AppCore installs default handlers
///   automatically for any interface you leave unset.
/// - **Applications using Renderer::Create() must register handlers manually.** The library
///   requires a file system at startup and a font loader when creating the first View (using stock
///   handlers or custom implementations), while a logger is optional.
///
/// ## Handler Lifecycle and Paths
///
/// Stock handlers follow these process-wide rules:
///
/// - **Never destroy the returned handlers.** The library owns each instance.
/// - **Each function returns a process-wide singleton.** A later call returns the existing instance
///   and ignores new path arguments.
/// - **Relative base directories resolve against the working directory.** Passing a relative path
///   to GetPlatformFileSystem() uses the process working directory (unlike
///   Settings::file_system_path, which resolves relative to the application bundle or executable).
///
/// @see  Platform::set_font_loader(), Platform::set_file_system(), Platform::set_logger(),
///       Renderer::Create(), App::Create()
///
#pragma once
#include "Defines.h"
#include <Ultralight/String.h>
#include <Ultralight/platform/FontLoader.h>
#include <Ultralight/platform/FileSystem.h>
#include <Ultralight/platform/Logger.h>

namespace ultralight {

///
/// Get the native font loader for the current platform.
///
/// @note  This singleton is owned by the library, do not destroy it.
///
AExport FontLoader* GetPlatformFontLoader();

///
/// Get the native file system for the current platform, creating it if it
/// doesn't exist using the base directory provided.
///
/// This is used to load data for file:/// URLs.
///
/// @param  baseDir  A base file path that will be used to resolve relative
///                  file paths. You can optionally specify "@resource_path"
///                  on macOS to use the app bundle's resource path.
///
/// @note  This singleton is owned by the library, do not destroy it.
///
AExport FileSystem* GetPlatformFileSystem(const String& baseDir);

///
/// Get the default logger (writes the log to a file on disk).
///
/// @param  logPath  A file path to write the log to.
///
/// \parblock
/// @note  This logger also shows a native message box for every LogLevel::Fatal message, so
///        log anything your app can recover from at a lower level.
/// \endparblock
///
/// \parblock
/// @note  This singleton is owned by the library, do not destroy it.
/// \endparblock
///
AExport Logger* GetDefaultLogger(const String& logPath);

}  // namespace ultralight
