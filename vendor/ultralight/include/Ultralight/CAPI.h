///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI.h
///
/// C API umbrella header. Include this single header to access the core Ultralight C API.
///
/// `#include <Ultralight/CAPI.h>`
///
/// The native DOM family stays opt-in and is not pulled in here. Include what you use:
/// `<Ultralight/CAPI/CAPI_DOMDocument.h>` and its `CAPI_DOM*` siblings for documents, nodes,
/// elements, events, ranges, and selection; `<Ultralight/CAPI/CAPI_DOMTriggers.h>` for
/// navigation-surviving page listeners; `<Ultralight/CAPI/CAPI_DOMData.h>` for declarative
/// data binding.
///
/// ## API Note
///
/// You should only destroy objects that you explicitly create. Do not destroy any objects returned
/// from the API or callbacks unless otherwise noted.
///
#ifndef ULTRALIGHT_CAPI_H
#define ULTRALIGHT_CAPI_H

#include <Ultralight/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_AudioOutput.h>
#include <Ultralight/CAPI/CAPI_Bitmap.h>
#include <Ultralight/CAPI/CAPI_Buffer.h>
#include <Ultralight/CAPI/CAPI_Clipboard.h>
#include <Ultralight/CAPI/CAPI_Color.h>
#include <Ultralight/CAPI/CAPI_Config.h>
#include <Ultralight/CAPI/CAPI_Editor.h>
#include <Ultralight/CAPI/CAPI_FileSystem.h>
#include <Ultralight/CAPI/CAPI_FontFile.h>
#include <Ultralight/CAPI/CAPI_FontLoader.h>
#include <Ultralight/CAPI/CAPI_Geometry.h>
#include <Ultralight/CAPI/CAPI_GPUDriver.h>
#include <Ultralight/CAPI/CAPI_ImageSource.h>
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSBuffer.h>
#include <Ultralight/CAPI/CAPI_JSClass.h>
#include <Ultralight/CAPI/CAPI_JSRuntime.h>
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/CAPI/CAPI_KeyEvent.h>
#include <Ultralight/CAPI/CAPI_Logger.h>
#include <Ultralight/CAPI/CAPI_MouseEvent.h>
#include <Ultralight/CAPI/CAPI_Platform.h>
#include <Ultralight/CAPI/CAPI_Renderer.h>
#include <Ultralight/CAPI/CAPI_ScrollEvent.h>
#include <Ultralight/CAPI/CAPI_GamepadEvent.h>
#include <Ultralight/CAPI/CAPI_Session.h>
#include <Ultralight/CAPI/CAPI_String.h>
#include <Ultralight/CAPI/CAPI_Surface.h>
#include <Ultralight/CAPI/CAPI_ThreadFactory.h>
#include <Ultralight/CAPI/CAPI_URL.h>
#include <Ultralight/CAPI/CAPI_View.h>

#endif // ULTRALIGHT_CAPI_H
