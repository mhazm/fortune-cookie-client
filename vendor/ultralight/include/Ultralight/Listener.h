///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/String.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/Buffer.h>
#include <Ultralight/ConsoleMessage.h>
#include <Ultralight/Editor.h>
#include <Ultralight/NetworkRequest.h>

namespace ultralight {

class View;


///
/// The mouse cursor types (see ViewListener::OnChangeCursor()).
///
enum Cursor {
  kCursor_Pointer = 0,
  kCursor_Cross,
  kCursor_Hand,
  kCursor_IBeam,
  kCursor_Wait,
  kCursor_Help,
  kCursor_EastResize,
  kCursor_NorthResize,
  kCursor_NorthEastResize,
  kCursor_NorthWestResize,
  kCursor_SouthResize,
  kCursor_SouthEastResize,
  kCursor_SouthWestResize,
  kCursor_WestResize,
  kCursor_NorthSouthResize,
  kCursor_EastWestResize,
  kCursor_NorthEastSouthWestResize,
  kCursor_NorthWestSouthEastResize,
  kCursor_ColumnResize,
  kCursor_RowResize,
  kCursor_MiddlePanning,
  kCursor_EastPanning,
  kCursor_NorthPanning,
  kCursor_NorthEastPanning,
  kCursor_NorthWestPanning,
  kCursor_SouthPanning,
  kCursor_SouthEastPanning,
  kCursor_SouthWestPanning,
  kCursor_WestPanning,
  kCursor_Move,
  kCursor_VerticalText,
  kCursor_Cell,
  kCursor_ContextMenu,
  kCursor_Alias,
  kCursor_Progress,
  kCursor_NoDrop,
  kCursor_Copy,
  kCursor_None,
  kCursor_NotAllowed,
  kCursor_ZoomIn,
  kCursor_ZoomOut,
  kCursor_Grab,
  kCursor_Grabbing,
  kCursor_Custom
};

///
/// User-defined interface to handle general events for a View.
///
/// Override the callbacks you need and pass an instance to View::set_view_listener(). Callbacks
/// run on the Renderer's thread.
///
class UExport ViewListener {
 public:
  virtual ~ViewListener() { }

  ///
  /// Called when the page title changes.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  title   The new title.
  ///
  virtual void OnChangeTitle([[maybe_unused]] ultralight::View* caller,
                             [[maybe_unused]] const String& title) { }

  ///
  /// Called when the main frame loads a new page.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  url     The new URL.
  ///
  virtual void OnChangeURL([[maybe_unused]] ultralight::View* caller,
                           [[maybe_unused]] const String& url) { }

  ///
  /// Called when the tooltip under the mouse changes.
  ///
  /// This is called as the mouse moves over the page, with the tooltip text of the element under
  /// the cursor (eg, its `title` attribute). It may repeat the same text on every move.
  ///
  /// The library doesn't display tooltips (and neither does AppCore)-- you should draw the text
  /// yourself near the cursor and hide it when the text is empty.
  ///
  /// @param  caller   The View that fired this event.
  ///
  /// @param  tooltip  The tooltip text (empty when there's no tooltip).
  ///
  virtual void OnChangeTooltip([[maybe_unused]] ultralight::View* caller,
                               [[maybe_unused]] const String& tooltip) { }

  ///
  /// Called when the mouse cursor changes.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  cursor  The new cursor.
  ///
  virtual void OnChangeCursor([[maybe_unused]] ultralight::View* caller,
                              [[maybe_unused]] Cursor cursor) { }

  ///
  /// Called when the page adds a message to the console (useful for errors and debugging).
  ///
  /// @param  caller   The View that fired this event.
  ///
  /// @param  message  The console message.
  ///
  virtual void OnAddConsoleMessage([[maybe_unused]] ultralight::View* caller,
                                   [[maybe_unused]] const ultralight::ConsoleMessage& message) { }

  ///
  /// Called when the page wants to open a new window (a link with `target="_blank"` or a call to
  /// `window.open()`).
  ///
  /// Return a new View to allow the window (eg, one from Renderer::CreateView()) or nullptr to
  /// block it. The library loads `target_url` into the View, and you display it.
  ///
  /// @param  caller      The View that fired this event.
  ///
  /// @param  opener_url  The URL of the page opening the window.
  ///
  /// @param  target_url  The URL the new window will load.
  ///
  /// @param  is_popup    Whether or not the page called `window.open()`.
  ///
  /// @param  popup_rect  The position and size the page asked for in `window.open()` (a value the
  ///                     page leaves out defaults to 0 for the position and 10 for the size).
  ///
  /// @return  Returns the new View, or nullptr to block the window.
  ///
  /// @warning  Keep a reference to the View you return for as long as it's open. The library
  ///           doesn't keep one.
  ///
  virtual RefPtr<View> OnCreateChildView(ultralight::View* caller, const String& opener_url,
                                         const String& target_url, bool is_popup,
                                         const IntRect& popup_rect);

  ///
  /// Called when the inspector needs a View to display in.
  ///
  /// This happens after you call View::CreateLocalInspectorView(), or when a remote inspector
  /// connects. Return a new View sized for your UI (eg, one from Renderer::CreateView()) or nullptr
  /// to cancel. The library loads the inspector into the View, and you display it.
  ///
  /// @param  caller         The View being inspected.
  ///
  /// @param  is_local       Whether this is a local inspector (true) or a remote inspector (false).
  ///
  /// @param  inspected_url  The URL of the page being inspected.
  ///
  /// @return  Returns the new View, or nullptr to cancel.
  ///
  virtual RefPtr<View> OnCreateInspectorView(ultralight::View* caller, bool is_local,
                                             const String& inspected_url);

  ///
  /// Called when the page asks to close (`window.close()` in a window that script opened, or in
  /// one with a single page in its history).
  ///
  /// @param  caller  The View that fired this event.
  ///
  virtual void OnRequestClose([[maybe_unused]] ultralight::View* caller) { }

};

///
/// User-defined interface to handle load-related events for a View.
///
/// Override the callbacks you need and pass an instance to View::set_load_listener(). Callbacks
/// run on the Renderer's thread.
///
/// A page load fires these events in order:
///
/// | Event                 | When                               | Use it to                  |
/// |-----------------------|------------------------------------|----------------------------|
/// | OnBeginLoading()      | The load starts                    | Show a loading indicator   |
/// | OnWindowObjectReady() | Before the page's scripts run      | Set up JavaScript state    |
/// | OnDOMReady()          | The document is parsed             | Read or change the DOM     |
/// | OnFinishLoading()     | The load ends                      | Hide the loading indicator |
/// | OnPageSettled()       | Loading and layout have gone quiet | Capture or show the page   |
///
/// If the load fails, OnFailLoading() comes just before OnFinishLoading().
///
/// ## Frames
///
/// OnWindowObjectReady() and OnDOMReady() are called for every frame on the page (check
/// `is_main_frame`). OnBeginLoading(), OnFinishLoading(), and OnFailLoading() are called once per
/// load for the frame that started it (usually the main frame).
///
/// @note  OnWindowObjectReady() and OnDOMReady() aren't called when a page is restored from the
///        back-forward cache.
///
class UExport LoadListener {
 public:
  virtual ~LoadListener() { }

  ///
  /// Called when a load starts.
  ///
  /// @param  caller         The View that fired this event.
  ///
  /// @param  frame_id       A unique ID for the frame.
  ///
  /// @param  is_main_frame  Whether or not this is the main frame.
  ///
  /// @param  url            The URL being loaded.
  ///
  virtual void OnBeginLoading([[maybe_unused]] ultralight::View* caller,
                              [[maybe_unused]] uint64_t frame_id,
                              [[maybe_unused]] bool is_main_frame,
                              [[maybe_unused]] const String& url) { }

  ///
  /// Called when a load ends (whether or not it succeeded).
  ///
  /// @param  caller         The View that fired this event.
  ///
  /// @param  frame_id       A unique ID for the frame.
  ///
  /// @param  is_main_frame  Whether or not this is the main frame.
  ///
  /// @param  url            The URL that was loaded.
  ///
  virtual void OnFinishLoading([[maybe_unused]] ultralight::View* caller,
                               [[maybe_unused]] uint64_t frame_id,
                               [[maybe_unused]] bool is_main_frame,
                               [[maybe_unused]] const String& url) { }

  ///
  /// Called when a load fails (just before OnFinishLoading()).
  ///
  /// HTTP error responses count as failures. Canceled loads aren't reported.
  ///
  /// @param  caller         The View that fired this event.
  ///
  /// @param  frame_id       A unique ID for the frame.
  ///
  /// @param  is_main_frame  Whether or not this is the main frame.
  ///
  /// @param  url            The URL that failed to load.
  ///
  /// @param  description    A human-readable description of the error.
  ///
  /// @param  error_domain   The kind of error ("HTTPErrorDomain" for an HTTP error response).
  ///
  /// @param  error_code     The error code (the HTTP status code for an HTTP error response).
  ///
  virtual void OnFailLoading([[maybe_unused]] ultralight::View* caller,
                             [[maybe_unused]] uint64_t frame_id,
                             [[maybe_unused]] bool is_main_frame,
                             [[maybe_unused]] const String& url,
                             [[maybe_unused]] const String& description,
                             [[maybe_unused]] const String& error_domain,
                             [[maybe_unused]] int error_code) { }

  ///
  /// Called before a frame's scripts run.
  ///
  /// Set up JavaScript state for the page here (APIs you added with View::AttachJSAPI() are already
  /// there). Use OnDOMReady() for anything that needs the DOM.
  ///
  /// @param  caller         The View that fired this event.
  ///
  /// @param  frame_id       A unique ID for the frame.
  ///
  /// @param  is_main_frame  Whether or not this is the main frame.
  ///
  /// @param  url            The URL of the frame's page.
  ///
  /// @note  Only called when JavaScript is enabled. A page with no scripts may never trigger it.
  ///
  virtual void OnWindowObjectReady([[maybe_unused]] ultralight::View* caller,
                                   [[maybe_unused]] uint64_t frame_id,
                                   [[maybe_unused]] bool is_main_frame,
                                   [[maybe_unused]] const String& url) { }

  ///
  /// Called when a frame's document has been parsed and the DOM is ready.
  ///
  /// This is the best time to read or change the DOM. The page's own `DOMContentLoaded` event has
  /// already fired. This is also called when JavaScript is disabled.
  ///
  /// @param  caller         The View that fired this event.
  ///
  /// @param  frame_id       A unique ID for the frame.
  ///
  /// @param  is_main_frame  Whether or not this is the main frame.
  ///
  /// @param  url            The URL of the frame's page.
  ///
  virtual void OnDOMReady([[maybe_unused]] ultralight::View* caller,
                          [[maybe_unused]] uint64_t frame_id,
                          [[maybe_unused]] bool is_main_frame,
                          [[maybe_unused]] const String& url) { }

  ///
  /// Called when the main frame's back-forward history changes.
  ///
  /// @param  caller  The View that fired this event.
  ///
  virtual void OnUpdateHistory([[maybe_unused]] ultralight::View* caller) { }

  ///
  /// Called when the page has settled and is ready to capture or display (resources have loaded,
  /// initial scripts have run, and layout is stable).
  ///
  /// This fires after OnFinishLoading() once network and layout activity have stayed idle for a
  /// short time (see Config::page_settle_delay).
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  url     The URL of the page that settled.
  ///
  /// @warning  Pages with constant network or layout activity (ads, animated layouts, streaming
  ///           video) may never settle. Use a timeout.
  ///
  /// \parblock
  /// @note  Animations that don't change layout (eg, a CSS transform or opacity transition) can
  ///        still be running when the page settles.
  /// \endparblock
  ///
  /// \parblock
  /// @note  You must call Renderer::Update(), Renderer::RefreshDisplay(), and Renderer::Render()
  ///        from your run loop for this to fire (App::Create() does this for you).
  /// \endparblock
  ///
  virtual void OnPageSettled([[maybe_unused]] ultralight::View* caller,
                             [[maybe_unused]] const String& url) { }
};

///
/// A unique ID for a download (see DownloadListener::NextDownloadId()).
///
typedef uint32_t DownloadId;

///
/// User-defined interface to handle downloads for a View.
///
/// A download starts when the page navigates to a file it can't display (eg, a response with
/// `Content-Disposition: attachment`). Downloads are ignored without a DownloadListener.
///
/// The library doesn't write anything to disk. You save the data yourself in these callbacks
/// (called in this order):
///
/// | Callback                   | What to do                         |
/// |----------------------------|------------------------------------|
/// | NextDownloadId()           | Return a new ID                    |
/// | OnRequestDownload()        | Return true to accept the download |
/// | OnBeginDownload()          | Open a file                        |
/// | OnReceiveDataForDownload() | Write the data to the file         |
/// | OnFinishDownload()         | Close the file                     |
/// | OnFailDownload()           | Close the file and delete it       |
///
/// Pass an instance to View::set_download_listener(). Callbacks run on the Renderer's thread.
///
class UExport DownloadListener {
 public:
  virtual ~DownloadListener() {}

  ///
  /// Called when the View needs an ID for a new download.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @return  Returns a new ID (a different one on every call, eg a counter starting at 0).
  ///
  virtual DownloadId NextDownloadId(ultralight::View* caller) = 0;

  ///
  /// Called when the page wants to download a file.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  id      The download's ID.
  ///
  /// @param  url     The URL of the file.
  ///
  /// @return  Returns true to allow the download, or false to block it.
  ///
  virtual bool OnRequestDownload(ultralight::View* caller, DownloadId id, const String& url) = 0;

  ///
  /// Called when a download starts.
  ///
  /// Open a file to write the data to here.
  ///
  /// @param  caller                   The View that fired this event.
  ///
  /// @param  id                       The download's ID.
  ///
  /// @param  url                      The URL of the file.
  ///
  /// @param  filename                 The suggested file name (from the server, or the end of the
  ///                                  URL).
  ///
  /// @param  expected_content_length  The expected size in bytes (as reported by the server).
  ///
  virtual void OnBeginDownload(ultralight::View* caller, DownloadId id, const String& url,
                               const String& filename, int64_t expected_content_length)  = 0;

  ///
  /// Called when a download receives data (usually several times per download).
  ///
  /// Write the data to the file here.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  id      The download's ID.
  ///
  /// @param  data    The data (only valid during this call, so copy it to keep it).
  ///
  virtual void OnReceiveDataForDownload(ultralight::View* caller, DownloadId id,
                                        RefPtr<Buffer> data) = 0;

  ///
  /// Called when a download finishes.
  ///
  /// Close the file here.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  id      The download's ID.
  ///
  virtual void OnFinishDownload(ultralight::View* caller, DownloadId id) = 0;

  ///
  /// Called when a download fails.
  ///
  /// Close the file and delete it here.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  id      The download's ID.
  ///
  virtual void OnFailDownload(ultralight::View* caller, DownloadId id) = 0;
};

///
/// User-defined interface to handle network requests for a View.
///
/// Override OnNetworkRequest() and pass an instance to View::set_network_listener(). It runs on
/// the Renderer's thread.
///
/// @pre  Not available in the Free edition (the listener is never called there).
///
class UExport NetworkListener {
 public:
  virtual ~NetworkListener() { }

  ///
  /// Called before the View sends a network request.
  ///
  /// Return false to block the request. You can also change `request` before it's sent.
  ///
  /// @param  caller   The View sending the request.
  ///
  /// @param  request  The request about to be sent.
  ///
  /// @return  Returns true to allow the request, or false to block it.
  ///
  /// @note  `file:` and `data:` URLs and synchronous XMLHttpRequests don't go through here.
  ///
  virtual bool OnNetworkRequest(ultralight::View* caller, NetworkRequest& request) = 0;
};

///
/// User-defined interface to handle editing events for a View.
///
/// Use it to turn an input method on and off and to keep its candidate window in place (see
/// Editor for the full input method steps). Override the callbacks you need and pass an instance
/// to View::set_editor_listener(). Callbacks run on the Renderer's thread.
///
/// @warning  With AppCore, each panel's View already has an EditorListener that runs the window's
///           input method. Replacing it breaks input method support for that View.
///
class UExport EditorListener {
 public:
  virtual ~EditorListener() { }

  ///
  /// Called when the editable state changes.
  ///
  /// This happens when focus moves to or from an editable element, when the View gains or loses
  /// focus, and when the focused element's `inputmode` changes. Use the state to turn an input
  /// method or on-screen keyboard on or off.
  ///
  /// @param  caller  The View that fired this event.
  ///
  /// @param  state   The new editable state.
  ///
  /// @warning  This can be called from inside your own View::Focus() or View::Unfocus() call. Don't
  ///           change focus or content from here (wait until the callback returns).
  ///
  virtual void OnChangeEditableState([[maybe_unused]] ultralight::View* caller,
                                     [[maybe_unused]] const EditableState& state) { }

  ///
  /// Called after every change to the active composition.
  ///
  /// Move the input method's candidate window here (see Editor::GetCompositionCharacterBounds()).
  ///
  /// @param  caller  The View that fired this event.
  ///
  virtual void OnUpdateComposition([[maybe_unused]] ultralight::View* caller) { }

  ///
  /// Called when the page ends the active composition on its own (eg, on navigation or when the
  /// selection moves away from it).
  ///
  /// Cancel the input method's composition here (on Windows, call ImmNotifyIME() with
  /// CPS_CANCEL). The composition's text stays in the page as final text, so don't insert it again.
  ///
  /// @param  caller  The View that fired this event.
  ///
  virtual void OnDiscardComposition([[maybe_unused]] ultralight::View* caller) { }
};

} // namespace ultralight
