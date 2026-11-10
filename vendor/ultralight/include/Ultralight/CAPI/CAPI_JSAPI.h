///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_JSAPI.h
///
/// A global JavaScript namespace attached to a View.
///
/// `#include <Ultralight/CAPI/CAPI_JSAPI.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// A ULJSAPI defines a global JavaScript namespace that connects your application's C code to page
/// scripts. Attaching the API to a View adds a frozen object to `window`, letting page scripts call
/// your callbacks and read properties just like built-in browser APIs.
///
/// This example sets up an API namespace and attaches it to a View:
///
/// ```c
/// static ULJSAPI g_api = NULL;  /* keep it while pages use the API */
///
/// /* app.getVolume() */
/// static ULJSValue OnGetVolume(void* user_data, ULJSContext ctx,
///                              ULJSValue this_value, const ULJSValue* args,
///                              size_t argc, ULJSValue* exception) {
///   return ulCreateJSValueNumber(ctx, 0.8);
/// }
///
/// void SetupApp(ULView view) {
///   g_api = ulCreateJSAPI("app");
///   ulJSAPIBindFunction(g_api, "getVolume", OnGetVolume, NULL, NULL);
///   ulJSAPISetConstantString(g_api, "version", "2.1.0");
///
///   if (ulViewAttachJSAPI(view, g_api, kULJSAPIAttachFlags_None, NULL, 0)) {
///     ULString url = ulCreateString("file:///app.html");
///     ulViewLoadURL(view, url);
///     ulDestroyString(url);
///   }
/// }
/// ```
///
/// Page script accesses the bindings through the global namespace object:
///
/// ```js
/// console.log(app.version);  // "2.1.0"
/// app.getVolume();           // 0.8
/// ```
///
/// ## Binding Functions and Values
///
/// The binding function you call determines what page script receives:
///
/// | Function                   | What Page Script Gets              |
/// |----------------------------|------------------------------------|
/// | ulJSAPIBindFunction()      | Function                           |
/// | ulJSAPIBindAsyncFunction() | Async function returning a Promise |
/// | ulJSAPIBindProperty()      | Live property                      |
/// | `ulJSAPISetConstant*()`    | Read-only constant                 |
/// | ulJSAPIRegisterClass()     | Constructor callable with `new`    |
///
/// A dot in a path creates nested namespace objects, so binding `"fs.readFile"` creates `app.fs`
/// and attaches the function to it.
///
/// Callbacks receive raw arguments without automatic count or type checking. You must check `argc`
/// and verify argument types with ulJSValueGetType() before reading or converting values. If an
/// argument doesn't match the expected type, create a TypeError using ulCreateJSMarshalError() and
/// store it in `*exception`.
///
/// This callback updates application volume when called by page script:
///
/// ```c
/// /* app.setVolume(level) */
/// static ULJSValue OnSetVolume(void* user_data, ULJSContext ctx,
///                              ULJSValue this_value, const ULJSValue* args,
///                              size_t argc, ULJSValue* exception) {
///   double level = 0;
///   if (argc < 1 || ulJSValueGetType(args[0]) != kULJSType_Number) {
///     *exception = ulCreateJSMarshalError(ctx, "app.setVolume", 1, NULL,
///                                         "number", argc ? args[0] : NULL);
///     return NULL;
///   }
///   ulJSValueToNumber(args[0], &level, NULL);
///   SetVolume(level);
///   return NULL;
/// }
/// ```
///
/// To free state associated with a binding, pass a `destroy_user_data` callback. The library calls
/// it once no page still holds a function created from that binding.
///
/// ## Attaching to a View
///
/// You must attach an API to a View before pages can access it. Call ulViewAttachJSAPI() before
/// loading content so every page the View loads gets the API.
///
/// Bindings added or removed after a page loads take effect on that page's next navigation.
///
/// Pass ULJSAPIAttachFlags to extend the API to subframes or leave namespace objects unfrozen so
/// page script can modify them.
///
/// @note  Once an API is attached to a View, you must bind and unbind on the Renderer's thread
///        only.
///
/// ## Controlling Page Access
///
/// By default, only local `file://` pages and HTML loaded with ulViewLoadHTML() or
/// ulViewLoadHTMLWithParams() get the API. Remote pages receive nothing.
///
/// Passing an array of origin rules to ulViewAttachJSAPI() replaces this default entirely. Only
/// pages matching at least one rule get the API (see ulViewAttachJSAPI() for rule syntax). Include
/// `"file://*"` in the array to keep local page access.
///
/// Pages loaded with ulViewLoadHTML() don't have a URL, so they receive an opaque security origin
/// that can't match any origin rule. To match rules against loaded HTML markup, call
/// ulViewLoadHTMLWithParams() with an explicit URL so the page adopts that URL's origin.
///
/// For decisions origin rules can't express, call ulViewSetJSAPIInjectionFilter() to set a filter
/// callback on the View. The View invokes the callback before adding the API to a page.
///
/// This helper allows access from a local development server alongside local files:
///
/// ```c
/// /* A local development server, plus your local pages: */
/// bool AttachForDevServer(ULView view) {
///   const char* rules[] = { "http://localhost:*", "file://*" };
///   return ulViewAttachJSAPI(view, g_api, kULJSAPIAttachFlags_None, rules, 2);
/// }
/// ```
///
/// ## Emitting Events
///
/// You can emit events from any thread using ulJSAPIEmitEventJSON() with arguments formatted as a
/// JSON array string. Events are delivered to attached pages on the Renderer's thread during a
/// later ulUpdate().
///
/// For arguments that JSON can't hold, call ulJSAPIEmitEvent() to produce values through a callback
/// on each receiving page.
///
/// This call notifies pages when a file write finishes:
///
/// ```c
/// void NotifySaved(void) {
///   ulJSAPIEmitEventJSON(g_api, "saved", "[\"slot1.dat\"]");
/// }
/// ```
///
/// Page script listens for events with on() and once(), and removes listeners with off().
///
/// The page registers a handler to refresh its state:
///
/// ```js
/// app.on("saved", (file) => refreshList(file));
/// ```
///
/// ## Asynchronous Functions and Promises
///
/// When page script calls a function bound with ulJSAPIBindAsyncFunction(), the page receives a
/// Promise immediately and your callback receives an owned ULJSPromiseResolver handle.
///
/// You can settle the resolver on the Renderer's thread using ulJSPromiseResolverResolve() or
/// ulJSPromiseResolverReject(), or from another thread using ulJSPromiseResolverComplete().
///
/// You must call ulDestroyJSPromiseResolver() exactly once for each resolver handle. Destroying an
/// unsettled resolver rejects the Promise with an error, so the awaiting page never hangs.
///
/// To observe a Promise returned by page script, call ulJSPromiseThen(). Your settlement callback
/// runs on the Renderer's thread during a later ulUpdate().
///
/// This asynchronous binding runs work on a background thread and completes the Promise when
/// finished:
///
/// ```c
/// typedef struct {
///   ULJSPromiseResolver resolver;
///   bool saved;
/// } SaveJob;
///
/// /* app.save() returns a Promise. */
/// static void OnSave(void* user_data, ULJSContext ctx, ULJSValue this_value,
///                    const ULJSValue* args, size_t argc,
///                    ULJSPromiseResolver resolver) {
///   SaveJob* job = (SaveJob*)calloc(1, sizeof(SaveJob));
///   job->resolver = resolver;
///   StartSave(job);  /* your save thread calls FinishSave() */
/// }
///
/// /* The Renderer's thread, during a later ulUpdate(). */
/// static ULJSValue MakeSaveResult(void* user_data, ULJSContext ctx,
///                                 ULJSValue* exception) {
///   return ulCreateJSValueBoolean(ctx, ((SaveJob*)user_data)->saved);
/// }
///
/// /* The save thread, when the write finishes. */
/// void FinishSave(SaveJob* job, bool saved) {
///   ULJSPromiseResolver resolver = job->resolver;
///   job->saved = saved;
///   ulJSPromiseResolverComplete(resolver, MakeSaveResult, job, free);
///   ulDestroyJSPromiseResolver(resolver);
/// }
/// ```
///
/// Page script awaits the returned Promise like any asynchronous call:
///
/// ```js
/// const saved = await app.save();
/// ```
///
/// ## Managing API Lifetime
///
/// You must keep the ULJSAPI handle alive while pages use the API. A View keeps the API attached
/// only while an owning handle exists.
///
/// When you destroy the last owning handle with ulDestroyJSAPI(), the API detaches from every View.
/// Events stop reaching pages, and calls to functions or class constructors that pages still hold
/// throw a TypeError with code `ULJS_DETACHED`.
///
/// This function destroys the API handle at application shutdown:
///
/// ```c
/// void ShutdownApp(void) {
///   ulDestroyJSAPI(g_api);  /* detaches the API from every View */
///   g_api = NULL;
/// }
/// ```
///
/// @note  For handle ownership, callback lifetimes, and threading rules shared across every C
///        JavaScript header, see CAPI_JSValue.h.
///
/// @see  ulCreateJSAPI(), ulJSAPIBindFunction(), ulJSAPIBindAsyncFunction(), ulViewAttachJSAPI(),
///       ulViewSetJSAPIInjectionFilter(), ulJSAPIEmitEventJSON(), ulCreateJSPromise(),
///       CAPI_JSValue.h, CAPI_JSClass.h
///
#ifndef ULTRALIGHT_CAPI_JSAPI_H
#define ULTRALIGHT_CAPI_JSAPI_H

#include <Ultralight/CAPI/CAPI_JSValue.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to a JavaScript API.
///
/// @see  ulCreateJSAPI(), ulDestroyJSAPI()
///
typedef struct C_JSAPI* ULJSAPI;

///
/// Opaque handle to the resolver that settles a pending Promise.
///
/// @see  ulCreateJSPromise(), ulDestroyJSPromiseResolver()
///
typedef struct C_JSPromiseResolver* ULJSPromiseResolver;

///
/// Opaque handle to a native class definition. See `<Ultralight/CAPI/CAPI_JSClass.h>`.
///
#ifndef ULTRALIGHT_ULJSCLASS_DEFINED
#define ULTRALIGHT_ULJSCLASS_DEFINED
typedef struct C_JSClass* ULJSClass;
#endif

///
/// Attachment options for ulViewAttachJSAPI().
///
typedef enum {
  kULJSAPIAttachFlags_None = 0,            ///< Main frame only, with frozen namespace objects.
  kULJSAPIAttachFlags_AllFrames = 1 << 1,  ///< Also add the API to subframes.
  kULJSAPIAttachFlags_Mutable = 1 << 2,    ///< Don't freeze namespace objects, so page script
                                           ///< can change or delete bindings (constants stay
                                           ///< read-only).
} ULJSAPIAttachFlags;

///
/// Callback invoked when JavaScript calls a function bound with ulJSAPIBindAsyncFunction().
///
/// The JavaScript caller receives a Promise right away, and you complete it through `resolver`.
///
/// @param  user_data   The user data passed to ulJSAPIBindAsyncFunction().
///
/// @param  ctx         The calling context (owned by the library, valid only during the callback;
///                     call ulCreateJSContextRef() to keep it).
///
/// @param  this_value  The call's `this` value (owned by the library, valid only during the
///                     callback).
///
/// @param  args        The call arguments (owned by the library, valid only during the callback;
///                     call ulCreateJSValueRef() to keep one).
///
/// @param  argc        The number of entries in `args`.
///
/// @param  resolver    The resolver for the caller's Promise. You own it: complete it (from any
///                     thread) and call ulDestroyJSPromiseResolver() when finished.
///
/// @note  Invoked on the Renderer's thread, while script is executing.
///
typedef void (*ULJSAsyncFunctionCallback)(void* user_data, ULJSContext ctx,
                                          ULJSValue this_value, const ULJSValue* args,
                                          size_t argc, ULJSPromiseResolver resolver);

///
/// Callback invoked on the Renderer's thread to produce the value for
/// ulJSPromiseResolverComplete().
///
/// @param  user_data  The user data passed to ulJSPromiseResolverComplete().
///
/// @param  ctx        The promise's context (owned by the library, live during the callback).
///
/// @param  exception  To reject the promise instead, set `*exception` to a handle you own (the
///                    library takes ownership).
///
/// @return  Return the value to resolve the promise with, as a handle you own (the library takes
///          ownership), or NULL for `undefined`.
///
typedef ULJSValue (*ULJSCompleteCallback)(void* user_data, ULJSContext ctx,
                                          ULJSValue* exception);

///
/// Callback invoked on the Renderer's thread when a promise observed with ulJSPromiseThen()
/// settles, or when its page goes away first.
///
/// @param  user_data  The user data passed to ulJSPromiseThen().
///
/// @param  ctx        The promise's context (owned by the library, valid only during the callback).
///
/// @param  result     The value or the rejection reason (owned by the library, valid only during
///                    the callback; call ulCreateJSValueRef() to keep it). NULL if the page went
///                    away before the promise settled.
///
/// @param  rejected   Whether the promise was rejected (true when `result` is NULL).
///
typedef void (*ULJSPromiseSettledCallback)(void* user_data, ULJSContext ctx, ULJSValue result,
                                           bool rejected);

///
/// Callback invoked when JavaScript reads a property bound with ulJSAPIBindProperty().
///
/// @param  user_data  The user data passed to ulJSAPIBindProperty().
///
/// @param  ctx        The calling context (owned by the library, valid only during the
///                    callback).
///
/// @param  exception  To throw an exception into JavaScript, set `*exception` to a handle you
///                    own; ownership transfers to the library and the return value is ignored.
///
/// @return  Return the property's value as a handle you own (ownership transfers to the
///          library), or NULL for `undefined`.
///
/// @note  Invoked on the Renderer's thread, while script is executing.
///
typedef ULJSValue (*ULJSAPIGetterCallback)(void* user_data, ULJSContext ctx,
                                           ULJSValue* exception);

///
/// Callback invoked when JavaScript assigns a property bound with ulJSAPIBindProperty().
///
/// @param  user_data  The user data passed to ulJSAPIBindProperty().
///
/// @param  ctx        The calling context (owned by the library, valid only during the
///                    callback).
///
/// @param  value      The assigned value (owned by the library, valid only during the callback;
///                    call ulCreateJSValueRef() to keep it).
///
/// @param  exception  To throw an exception into JavaScript, set `*exception` to a handle you
///                    own; ownership transfers to the library.
///
/// @return  Return true to accept the assignment. Return false to reject it, which throws a
///          TypeError into the page (unless you set `*exception`).
///
/// @note  Invoked on the Renderer's thread, while script is executing.
///
typedef bool (*ULJSAPISetterCallback)(void* user_data, ULJSContext ctx, ULJSValue value,
                                      ULJSValue* exception);

///
/// Callback invoked to create the arguments of an event emitted with ulJSAPIEmitEvent(), once
/// for each page that receives it.
///
/// @param  user_data  The user data passed to ulJSAPIEmitEvent().
///
/// @param  ctx        The context of the page receiving the event (owned by the library, valid
///                    only during the callback).
///
/// @param  args       An array of `max_args` entries to fill with handles you own. Ownership of
///                    each filled entry transfers to the library, and a NULL entry is passed
///                    as `undefined`.
///
/// @param  max_args   The number of entries in `args` (16).
///
/// @return  Return the number of arguments you filled (at most `max_args`).
///
/// @note  Invoked on the Renderer's thread.
///
typedef size_t (*ULJSEventArgsCallback)(void* user_data, ULJSContext ctx, ULJSValue* args,
                                        size_t max_args);

///
/// The details a JavaScript API injection filter decides on (see
/// ULJSAPIInjectionFilterCallback).
///
/// New members may be added at the end in later versions, so don't create or copy this struct
/// yourself.
///
typedef struct ULJSAPIInjectionRequest {
  ///
  /// The View loading the page.
  ///
  ULView view;

  ///
  /// The JavaScript API being decided on. It's the handle ulCreateJSAPI() returned, so you can
  /// compare it with yours. It's valid only during the callback (call
  /// ulCreateJSAPIBorrowedRef() to keep it).
  ///
  ULJSAPI api;

  ///
  /// The page's origin, serialized (eg, "https://example.com"). Opaque origins serialize as
  /// "null".
  ///
  const char* origin;

  ///
  /// Whether or not the page is in the View's main frame.
  ///
  bool is_main_frame;

  ///
  /// Whether or not the origin rules admit the page.
  ///
  bool rules_allow;
} ULJSAPIInjectionRequest;

///
/// Callback invoked to decide whether a JavaScript API is added to a page, after the origin
/// rules have been checked.
///
/// The verdict of the origin rules arrives in `request->rules_allow`, and your return value is
/// final in both directions. Use this for policy the origin rules can't express, such as a user
/// setting, or to admit a page with an opaque origin (eg, a sandboxed frame).
///
/// @param  user_data  The user data passed to ulViewSetJSAPIInjectionFilter().
///
/// @param  request    The API, the page, and the verdict of the origin rules. It's owned by the
///                    library and valid only during the callback.
///
/// @return  Return true to add the API to the page, or false to withhold it.
///
/// @note  Invoked on the Renderer's thread, for the main frame and (when the API is attached
///        with kULJSAPIAttachFlags_AllFrames) for subframes.
///
typedef bool (*ULJSAPIInjectionFilterCallback)(void* user_data,
                                               const ULJSAPIInjectionRequest* request);

/******************************************************************************
 * JSAPI lifetime and registration
 *****************************************************************************/

///
/// Create a new JavaScript API rooted at a global namespace path.
///
/// @param  root_path  The dot-separated namespace path (eg, "myApp" or "myApp.native"). Each
///                    segment must be non-empty. The roots "ul" and "ultralight", and every
///                    path under them, are reserved for the library.
///
/// @return  Returns a new ULJSAPI instance, or NULL if `root_path` is NULL, empty, has an empty
///          segment, or is reserved. You must call ulDestroyJSAPI() when finished.
///
ULExport ULJSAPI ulCreateJSAPI(const char* root_path);

///
/// Take an additional owning reference to a JavaScript API.
///
/// Use this when two owners manage the API's lifetime independently. Each reference is
/// released with its own ulDestroyJSAPI() call.
///
/// @param  api  The JavaScript API (may be NULL).
///
/// @return  Returns an owning handle to the API (NULL for a NULL `api`). It's the same handle
///          ulCreateJSAPI() returned.
///
/// @note  Safe to call from any thread.
///
ULExport ULJSAPI ulCreateJSAPIRef(ULJSAPI api);

///
/// Take a reference to a JavaScript API that doesn't own it.
///
/// A borrowed handle works with every function in this header, but it doesn't keep the API
/// attached: once the last owning handle is destroyed, the API detaches from its Views as
/// ulDestroyJSAPI() describes, and the borrowed handle stays safe to use (its calls have no
/// effect on any page). Use it to keep an API you were handed (eg, in an injection filter)
/// without extending how long it stays attached.
///
/// @param  api  The JavaScript API (may be NULL).
///
/// @return  Returns a new borrowed handle (NULL for a NULL `api`). You must call
///          ulDestroyJSAPI() when finished. It's a different handle than `api`, so compare
///          the handles you own instead.
///
/// @note  Safe to call from any thread.
///
ULExport ULJSAPI ulCreateJSAPIBorrowedRef(ULJSAPI api);

///
/// Destroy a JavaScript API handle (NULL-safe).
///
/// The View keeps the API attached while an owning handle to it exists (one from
/// ulCreateJSAPI() or ulCreateJSAPIRef()). When you destroy the last one:
///
///  - **The API detaches from every View**, and native events stop reaching pages.
///  - **Functions the pages still hold throw** a TypeError with code `ULJS_DETACHED` instead of
///    calling your callbacks. So do the constructors and static methods of the classes it
///    registered, once no other API with an owning handle registered them too.
///  - **Resolvers your async functions received are detached** (see
///    ulJSPromiseResolverIsDetached()), so work still running for them can stop early.
///  - **Each binding's `destroy_user_data` runs** on the Renderer's thread: during this call
///    when you make it there and nothing else refers to the API, otherwise during a later
///    ulUpdate().
///
/// @param  api  The JavaScript API handle.
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyJSAPI(ULJSAPI api);

///
/// Get the root namespace path the JavaScript API was created with.
///
/// @param  api  The JavaScript API.
///
/// @return  Returns the root path (NULL for a NULL `api`). You must call ulDestroyString() when
///          finished.
///
ULExport ULString ulJSAPIGetRootPath(ULJSAPI api);

///
/// Bind a function at a path.
///
/// Calling the function from JavaScript invokes `callback` and returns its result. Binding a
/// path that's already bound replaces the previous binding.
///
/// @param  api                The JavaScript API.
///
/// @param  path               The dot-separated path relative to the root (eg, "greet" or
///                            "fs.readFile"). Each segment must be non-empty.
///
/// @param  callback           The callback to invoke when JavaScript calls the function.
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` (may be
///                            NULL). It runs after the binding is replaced, unbound, or its API
///                            destroyed, once no page still holds a function created from it.
///                            If nothing is bound (NULL `api` or `callback`, or an invalid
///                            `path`), it runs immediately. It may run on any thread.
///
/// @return  Returns true if the binding was added (false if `api` or `callback` is NULL or
///          `path` is invalid; a warning is logged).
///
ULExport bool ulJSAPIBindFunction(ULJSAPI api, const char* path, ULJSFunctionCallback callback,
                                  void* user_data,
                                  ULUserDataDestroyCallback destroy_user_data);

///
/// Bind an asynchronous function at a path.
///
/// Calling the function from JavaScript returns a Promise right away and invokes `callback`
/// with the Promise's resolver:
///
/// ```
/// void OnQuery(void* user_data, ULJSContext ctx, ULJSValue this_value,
///              const ULJSValue* args, size_t argc, ULJSPromiseResolver resolver) {
///   // Hand `resolver` to a worker thread; complete and destroy it there when done.
/// }
///
/// ulJSAPIBindAsyncFunction(api, "db.query", OnQuery, NULL, NULL);
/// // Page script:  const rows = await myApp.db.query("...");
/// ```
///
/// The callback runs during the JavaScript call, so keep it short and do slow work elsewhere
/// (see ulJSRunOnWorker()). Destroying the resolver without completing it rejects the Promise,
/// so an awaiting page never hangs.
///
/// @param  api                The JavaScript API.
///
/// @param  path               The dot-separated path relative to the root (eg, "db.query").
///                            Each segment must be non-empty.
///
/// @param  callback           The callback to invoke when JavaScript calls the function.
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` (may be
///                            NULL). It runs at the same point as for ulJSAPIBindFunction().
///
/// @return  Returns true if the binding was added (false if `api` or `callback` is NULL or
///          `path` is invalid; a warning is logged).
///
ULExport bool ulJSAPIBindAsyncFunction(ULJSAPI api, const char* path,
                                       ULJSAsyncFunctionCallback callback, void* user_data,
                                       ULUserDataDestroyCallback destroy_user_data);

///
/// Bind an accessor property at a path.
///
/// JavaScript reads invoke the getter and assignments invoke the setter, so the page always
/// sees live native state:
///
/// ```
/// ulJSAPIBindProperty(api, "volume", OnGetVolume, OnSetVolume, NULL, NULL);
/// // Page script:  myApp.volume        -> OnGetVolume
/// //               myApp.volume = 0.5  -> OnSetVolume
/// ```
///
/// @param  api                The JavaScript API.
///
/// @param  path               The dot-separated path relative to the root (eg, "volume").
///                            Each segment must be non-empty.
///
/// @param  getter             The getter callback (required).
///
/// @param  setter             The setter callback, or NULL to make the property read-only
///                            (assignment then throws in strict mode and is ignored otherwise).
///
/// @param  user_data          Opaque user data passed to every invocation of `getter` and
///                            `setter`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` (may be
///                            NULL). It runs at the same point as for ulJSAPIBindFunction().
///
/// @return  Returns true if the binding was added (false if `api` or `getter` is NULL or
///          `path` is invalid; a warning is logged).
///
ULExport bool ulJSAPIBindProperty(ULJSAPI api, const char* path, ULJSAPIGetterCallback getter,
                                  ULJSAPISetterCallback setter, void* user_data,
                                  ULUserDataDestroyCallback destroy_user_data);

///
/// Set a constant number at a path.
///
/// Constants are read-only from JavaScript, even when the API is attached with
/// kULJSAPIAttachFlags_Mutable. Setting a path that's already bound replaces the previous
/// binding.
///
/// @param  api    The JavaScript API.
///
/// @param  path   The dot-separated path relative to the root (eg, "maxPlayers"). Each segment
///                must be non-empty.
///
/// @param  value  The value.
///
/// @return  Returns true if the constant was set (false if `api` is NULL or `path` is invalid;
///          a warning is logged).
///
ULExport bool ulJSAPISetConstantNumber(ULJSAPI api, const char* path, double value);

///
/// Set a constant boolean at a path (see ulJSAPISetConstantNumber()).
///
/// @param  api    The JavaScript API.
///
/// @param  path   The dot-separated path relative to the root.
///
/// @param  value  The value.
///
/// @return  Returns true if the constant was set (false if `api` is NULL or `path` is invalid;
///          a warning is logged).
///
ULExport bool ulJSAPISetConstantBoolean(ULJSAPI api, const char* path, bool value);

///
/// Set a constant string at a path (see ulJSAPISetConstantNumber()).
///
/// @param  api    The JavaScript API.
///
/// @param  path   The dot-separated path relative to the root.
///
/// @param  value  The value as a null-terminated UTF-8 string (NULL is ignored).
///
/// @return  Returns true if the constant was set (false if `api` or `value` is NULL or `path`
///          is invalid; a warning is logged).
///
ULExport bool ulJSAPISetConstantString(ULJSAPI api, const char* path, const char* value);

///
/// Set a constant at a path from a JSON string, for structured values.
///
/// ```
/// ulJSAPISetConstantJSON(api, "buildInfo", "{\"channel\":\"beta\",\"build\":1042}");
/// // Page script:  myApp.buildInfo.channel  -> "beta"
/// ```
///
/// The whole value is read-only from JavaScript (every object and array in it is frozen). The
/// JSON is parsed as each page loads. If it doesn't parse, the page doesn't get the constant
/// and a warning is logged.
///
/// @param  api   The JavaScript API.
///
/// @param  path  The dot-separated path relative to the root.
///
/// @param  json  The value as a null-terminated JSON string (NULL is ignored).
///
/// @return  Returns true if the constant was set (false if `api` or `json` is NULL or `path` is
///          invalid; a warning is logged). JSON that doesn't parse is reported later, as each
///          page loads.
///
ULExport bool ulJSAPISetConstantJSON(ULJSAPI api, const char* path, const char* json);

///
/// Register a class at a path.
///
/// The class's constructor becomes reachable at the path, so page script can call
/// `new myApp.Database(...)`, use its static methods, and test instances with `instanceof`.
/// See `<Ultralight/CAPI/CAPI_JSClass.h>` for defining classes.
///
/// Registering seals the class definition (see ulCreateJSClass()). Registering a path that's
/// already bound replaces the previous binding.
///
/// @param  api   The JavaScript API.
///
/// @param  path  The dot-separated path relative to the root (eg, "Database"). Each segment
///               must be non-empty.
///
/// @param  cls   The class definition. The API keeps its own reference, so you can destroy
///               yours after this call.
///
/// @return  Returns true if the class was registered (false if `api` or `cls` is NULL or `path`
///          is invalid; a warning is logged).
///
/// @note  The constructor and its prototype aren't frozen, so page script can extend them.
///        Those changes stay on that page and never reach native code.
///
ULExport bool ulJSAPIRegisterClass(ULJSAPI api, const char* path, ULJSClass cls);

///
/// Remove the binding at a path, and any metadata set at that path.
///
///  - **Loaded pages** keep the binding until they navigate, and so does a page restored from
///    the back-forward cache.
///  - **User data**: the binding's `destroy_user_data` runs once no page still holds a
///    function created from the binding (immediately if none does).
///  - **Calls in progress** finish normally: the resolvers your async functions received don't
///    become detached (see ulJSPromiseResolverIsDetached()).
///  - **Events** declared with ulJSAPISetMetadata() on the empty path are never removed.
///  - **`on`, `once`, `off`**: removing your own binding at one of these names brings back the
///    built-in event method on pages loaded afterward.
///
/// @param  api   The JavaScript API.
///
/// @param  path  The dot-separated path relative to the root (an empty path is refused).
///
/// @return  Returns whether or not a binding or metadata was removed (false when there was
///          nothing at `path`, or `path` is NULL or empty).
///
ULExport bool ulJSAPIUnbind(ULJSAPI api, const char* path);

///
/// Add documentation or type metadata for a path, as a JSON object.
///
/// ulJSAPIGetSchema() merges the metadata into its output:
///
///  - **Merging**: nested objects merge field by field. Other values (strings, numbers,
///    arrays) replace.
///  - **Layering**: setting the same path again layers the new object over the earlier ones,
///    and later values win.
///  - **Rebinding**: binding a path again (replacing its binding) removes that path's metadata,
///    so set metadata after the binding it describes.
///  - **Paths with no binding** get an entry that holds only the metadata.
///  - **The empty path** is the whole document. Its `meta`, `events`, and `types` sections
///    hold document-level information.
///
/// You declare events on the empty path, keyed by their path relative to the root:
///
/// ```
/// ulJSAPISetMetadata(api, "", "{\"events\": {\"saved\": {}, \"fs.changed\": {}}}");
/// ```
///
/// Once an API declares an event, kULJSDiagnostics_Warn reports subscriptions to and emits of
/// events it didn't declare.
///
/// @param  api            The JavaScript API.
///
/// @param  path           The dot-separated path relative to the root, or an empty string for
///                        the whole document.
///
/// @param  metadata_json  A JSON object as a null-terminated string. Anything else is ignored
///                        with a logged warning.
///
ULExport void ulJSAPISetMetadata(ULJSAPI api, const char* path, const char* metadata_json);

///
/// Get the JavaScript API's schema as JSON, for tooling (eg, generating TypeScript
/// declarations).
///
/// The JSON has a `format` ("ul-schema"), an `api` ("js"), a `version`, the root path, an entry
/// for each binding under `bindings` (its kind, plus the value of a constant), the shape of each
/// registered class under `types`, and the metadata added with ulJSAPISetMetadata(). Empty
/// sections are left out, and the same registrations always give the same output.
///
/// A reader should ignore keys and types it doesn't know. The `version` changes only for a
/// change that would break an existing reader.
///
/// @param  api  The JavaScript API.
///
/// @return  Returns the JSON text (NULL for a NULL `api`). You must call ulDestroyString() when
///          finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULString ulJSAPIGetSchema(ULJSAPI api);

///
/// Write the JSON from ulJSAPIGetSchema() to a file.
///
/// The library writes the file before this returns.
///
/// @param  api   The JavaScript API.
///
/// @param  path  The file's path as a UTF-8 string (on Windows too). An existing file is
///               replaced.
///
/// @return  Returns whether or not the file was written (false for a NULL `api`, or a NULL or
///          empty `path`). A write that fails also logs a warning with the path.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulJSAPIDumpSchema(ULJSAPI api, const char* path);

/******************************************************************************
 * Diagnostics
 *****************************************************************************/

///
/// The various diagnostics levels for a JavaScript API.
///
/// Diagnostics are development warnings. Most go to both the native log and the page console
/// (as kMessageSource_NativeAPI messages, with the script line that caused them). Off, Warn,
/// and Strict are ordered, so you can compare a level in effect (eg,
/// `ulJSAPIGetCallDiagnostics() >= kULJSDiagnostics_Warn`). kULJSDiagnostics_Auto is only a
/// setting: the functions that get a level never return it.
///
/// **Warn** reports likely mistakes involving the API:
///
///  - **Missing properties**: page script reads a property that doesn't exist on one of the
///    API's namespace objects (with a did-you-mean suggestion).
///  - **Undeclared events**: page script subscribes to, or native code emits, an event the API
///    didn't declare (only once it declares at least one; see ulJSAPISetMetadata()).
///  - **Withheld pages**: a page didn't get the API because of the origin rules or the View's
///    filter (with the page's origin and the reason).
///  - **Reserved member names**: you register a member named `then`, `toJSON`, `constructor`,
///    or `__proto__`, which changes how JavaScript treats the object.
///
/// These process-wide warnings follow the Config's JavaScript diagnostics level (see
/// ulConfigSetJavaScriptDiagnostics()) rather than the level of any one API:
///
///  - **Gone pages**: the first operation on a handle whose page is gone logs why its page went
///    away (once per page).
///  - **Unread exceptions**: an exception from a call that got no `exception` out-parameter is
///    logged with its message and stack.
///  - **Handle origins**: debug builds of the library record where each handle was created, so
///    a handle-misuse abort can report it.
///
/// **Strict** does everything Warn does, and tells bindings to refuse caller mistakes (such as
/// a lossy numeric argument or extra arguments) with a TypeError. The C++ bindings in
/// `<Ultralight/js/API.h>` apply these checks for you. C callbacks receive the raw arguments,
/// so check ulJSAPIGetCallDiagnostics() to apply your own.
///
/// Mistakes the library finds once, like an invalid root or binding path, are always logged,
/// whatever the level.
///
typedef enum {
  kULJSDiagnostics_Off = 0,  ///< No development warnings and no added cost.
  kULJSDiagnostics_Warn,     ///< Report likely mistakes.
  kULJSDiagnostics_Strict,   ///< Warn, plus bindings refuse caller mistakes.
  kULJSDiagnostics_Auto,     ///< Use the Config's JavaScript diagnostics level.
} ULJSDiagnosticsLevel;

///
/// Set the JavaScript API's diagnostics level (Default = kULJSDiagnostics_Auto).
///
/// Warn and Strict slow down property reads on the API's namespace objects, and attaching an
/// API at Warn or higher logs a notice saying so. The default, kULJSDiagnostics_Auto, uses the
/// Config's level (see ulConfigSetJavaScriptDiagnostics()), which is Warn in developer mode and
/// Off otherwise.
///
/// @param  api    The JavaScript API.
///
/// @param  level  The new level.
///
/// \parblock
/// @note  Set the level before attaching the API to a View. Raising it turns on
///        missing-property warnings only for pages loaded afterward, and the notice and the
///        automatic attach of the diagnostics API (see ulJSAPIGetDiagnosticsAPI()) happen only
///        when you attach. Other warnings follow the level right away.
/// \endparblock
///
/// \parblock
/// @note  The UL_JS_DIAGNOSTICS environment variable ("off", "warn", or "strict", any case)
///        overrides the level of every JavaScript API while developer mode is on (see
///        ulConfigSetDeveloperMode()), so you can switch diagnostics without recompiling. The
///        library reads it once, at first use.
/// \endparblock
///
ULExport void ulJSAPISetDiagnostics(ULJSAPI api, ULJSDiagnosticsLevel level);

///
/// Get the JavaScript API's effective diagnostics level.
///
/// @param  api  The JavaScript API.
///
/// @return  Returns the level in effect: the UL_JS_DIAGNOSTICS override when it applies, else
///          the level set with ulJSAPISetDiagnostics(), with kULJSDiagnostics_Auto replaced by
///          the Config's level (this is never kULJSDiagnostics_Auto). A NULL `api` gets the
///          Config's level.
///
ULExport ULJSDiagnosticsLevel ulJSAPIGetDiagnostics(ULJSAPI api);

///
/// Get the diagnostics level for the binding callback that's running.
///
/// Call this from a binding callback to decide whether to warn about or refuse a caller
/// mistake:
///
///  - **In a callback bound with this header's functions**: the effective level of that
///    binding's JavaScript API.
///  - **In a class callback** (method, static method, constructor, or accessor): the Config's
///    JavaScript diagnostics level (or the UL_JS_DIAGNOSTICS override), since a class can be
///    registered with several APIs.
///  - **Outside any binding callback**: the Config's JavaScript diagnostics level (or the
///    UL_JS_DIAGNOSTICS override).
///
/// @return  Returns the level.
///
/// @note  Must be called on the Renderer's thread.
///
ULExport ULJSDiagnosticsLevel ulJSAPIGetCallDiagnostics(void);

///
/// Log a message the way the library logs its own diagnostics.
///
/// The message goes to the native log as a warning and, when `ctx` is alive, to that page's
/// console as a kMessageSource_NativeAPI message (with the script line that's running, if any).
/// It's always logged, so check ulJSAPIGetCallDiagnostics() first. You should start the
/// message with the binding path.
///
/// @param  ctx      The context whose page console gets the message (NULL or gone to log to
///                  the native log only).
///
/// @param  message  The message as a null-terminated UTF-8 string.
///
/// @note  Must be called on the Renderer's thread.
///
ULExport void ulJSAPIEmitDiagnostic(ULJSContext ctx, const char* message);

///
/// Get the library's diagnostics API, which lets page script inspect the JavaScript APIs
/// added to its page.
///
/// Its root is `ul.diagnostics`:
///
/// ```
/// ul.diagnostics.apis()          // The roots of the APIs added to this page
/// ul.diagnostics.schema(root)    // That API's ulJSAPIGetSchema() JSON as an object, or null
/// ul.diagnostics.level(root)     // That API's diagnostics level ("off", "warn", "strict")
/// ul.diagnostics.stats()         // Counts of the native handles into this page
/// ```
///
/// A page only sees the APIs it was given: `schema()` and `level()` return null for any other
/// root.
///
/// Attach it with ulViewAttachJSAPI() like any other API. Attaching any API at Warn or Strict
/// also attaches this one to the View, with the combined flags and origin rules of the View's
/// APIs at Warn or Strict (so it reaches the pages they reach). At Off, a View exposes it only
/// when you attach it yourself.
///
/// @return  Returns a new handle to the diagnostics API. You must call ulDestroyJSAPI() when
///          finished (the API itself stays alive for the life of the process).
///
ULExport ULJSAPI ulJSAPIGetDiagnosticsAPI(void);

/******************************************************************************
 * Attachment
 *****************************************************************************/

///
/// Attach a JavaScript API to a View.
///
/// The library adds the API to the View's current page (if the origin rules admit it) and to
/// every page the View loads afterward. The View keeps the API attached while an owning handle
/// to it exists (see ulDestroyJSAPI()). Attaching an API that's already attached replaces its
/// flags and origin rules (a page that already has the API keeps it as it is).
///
/// ## Origin Rules
///
/// `origin_rules` lists the pages that get the API. Each rule has the form
/// `scheme://host[:port]`:
///
///  - **Scheme**: required and matched exactly (case doesn't matter). There's no wildcard
///    scheme, so write out each scheme you allow.
///  - **Host**: an exact host, `*` for any host, or `*.example.com` for example.com and all
///    of its subdomains. Hosts are normalized like URLs (lowercase, punycode), so international
///    domain names work.
///  - **Port**: optional. No port matches only the scheme's default port, `:8443` matches that
///    port, and `:*` matches any port.
///
/// ```
/// "https://store.mygame.com"   // store.mygame.com on the default port (443)
/// "https://*.mygame.com"       // mygame.com and all of its subdomains
/// "http://localhost:*"         // localhost on any port
/// "file://*"                   // local files
/// ```
///
/// Rules match the page's security origin, not its URL, so frames that inherit an origin
/// (`about:blank`, `srcdoc`) are matched by the origin they inherit. Pages with an opaque
/// origin (eg, sandboxed frames, `data:` URLs, and custom schemes) match no rule. Use the
/// View's filter (see ulViewSetJSAPIInjectionFilter()) to admit them.
///
/// ## The Default Rules
///
/// With no rules, the API goes only to your application's own content: local `file://` pages,
/// pages loaded with ulViewLoadHTML() or ulViewLoadHTMLWithParams(), and subframes with the
/// same origin as those pages. Remote pages get nothing.
///
/// Passing any rules replaces the default entirely. A page loaded with ulViewLoadHTML() has
/// an opaque origin, so no rule matches it (use the filter to admit it).
///
/// A page served by a development server (eg, `http://localhost:5173`) is remote, so it needs
/// a rule such as `http://localhost:*`. The first page the rules keep from a View is always
/// logged (at Info when diagnostics are off), so you can find out why a page has no API.
///
/// @param  view              The View.
///
/// @param  api               The JavaScript API to attach.
///
/// @param  flags             A bitwise OR of ULJSAPIAttachFlags values. Unknown bits fail the
///                           attach.
///
/// @param  origin_rules      An array of `num_origin_rules` rules, or NULL for the default
///                           rules.
///
/// @param  num_origin_rules  The number of entries in `origin_rules` (0 for the default rules).
///
/// @return  Returns true on success. Returns false if `view` or `api` is NULL, `flags` has an
///          unknown bit, `origin_rules` is NULL but `num_origin_rules` isn't 0, a rule fails to
///          parse, or the API's last owning handle was destroyed. Nothing changes then, and a
///          logged warning says why.
///
/// \parblock
/// @note  Call this on the Renderer's thread.
/// \endparblock
///
/// \parblock
/// @note  Attach APIs that share a namespace prefix (eg, roots `myApp.audio` and
///        `myApp.video`) before the page loads. An API attached later can't add to a namespace
///        object the current page has already frozen, so it appears from the next page instead
///        (and a warning is logged).
/// \endparblock
///
/// \parblock
/// @note  A page restored from the back-forward cache (see ulConfigSetPageCacheSize()) gets the
///        API back: its namespace and the function references its scripts kept work again, and
///        events reach it. Two things don't survive the round trip:
///
///         - **Event subscriptions** made with `on` or `once` (page script should subscribe
///           again in a `pageshow` handler).
///         - **Class instances** created before the page was cached (using one throws).
/// \endparblock
///
ULExport bool ulViewAttachJSAPI(ULView view, ULJSAPI api, unsigned flags,
                                const char* const* origin_rules, size_t num_origin_rules);

///
/// Detach a JavaScript API from a View.
///
/// The current page keeps the API until it navigates, but events stop reaching the View's
/// pages right away.
///
/// @param  view  The View.
///
/// @param  api   The JavaScript API to detach.
///
/// @note  Call this on the Renderer's thread.
///
ULExport void ulViewDetachJSAPI(ULView view, ULJSAPI api);

///
/// Set the View's filter for deciding which pages get each JavaScript API.
///
/// The View consults the filter after the origin rules, every time an attached API could be
/// added to a page.
///
/// @param  view               The View.
///
/// @param  callback           The filter (see ULJSAPIInjectionFilterCallback), or NULL to
///                            remove the current one.
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` when the
///                            filter is replaced or removed, or the View is destroyed (may be
///                            NULL). When `callback` is NULL, it runs immediately.
///
/// \parblock
/// @note  Call this on the Renderer's thread.
/// \endparblock
///
/// \parblock
/// @note  An API the filter detaches can still be added to the page being decided on. The
///        change applies from the next page load.
/// \endparblock
///
ULExport void ulViewSetJSAPIInjectionFilter(ULView view, ULJSAPIInjectionFilterCallback callback,
                                            void* user_data,
                                            ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Events
 *****************************************************************************/

///
/// Emit an event to page script with arguments given as a JSON array.
///
/// Page script subscribes with the `on` or `once` method of a namespace object, and the event
/// path is relative to the root:
///
/// ```
/// // Page script:
/// myApp.on('changed', (what) => { ... });      // receives "changed"
/// myApp.fs.on('changed', (path) => { ... });   // receives "fs.changed"
///
/// // Native code:
/// ulJSAPIEmitEventJSON(api, "fs.changed", "[\"C:/saves/slot1.dat\"]");
/// ```
///
/// Each page that has the API when you emit gets the event during a later ulUpdate().
/// Listeners are called with the global object as `this`. A listener that throws doesn't stop
/// the others (a warning is logged).
///
/// @param  api         The JavaScript API.
///
/// @param  event_path  The event path relative to the root (eg, "changed" or "fs.changed").
///
/// @param  args_json   A JSON array of arguments, or NULL for none. Anything other than a JSON
///                     array delivers the event with no arguments and logs a warning.
///
/// @note  Safe to call from any thread.
///
ULExport void ulJSAPIEmitEventJSON(ULJSAPI api, const char* event_path, const char* args_json);

///
/// Emit an event to page script with arguments created by a callback.
///
/// Use this instead of ulJSAPIEmitEventJSON() for arguments that can't go through JSON (eg,
/// existing objects or large buffers). The library invokes `args_callback` on the Renderer's
/// thread once for each page that gets the event, and it can create up to 16 arguments. Delivery
/// otherwise matches ulJSAPIEmitEventJSON().
///
/// @param  api                The JavaScript API.
///
/// @param  event_path         The event path relative to the root (eg, "changed" or
///                            "fs.changed").
///
/// @param  args_callback      The callback that creates the arguments for each page.
///
/// @param  user_data          Opaque user data passed to every invocation of `args_callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after every
///                            delivery has run or been skipped (may be NULL). When no page has
///                            the API, it runs immediately. It may run on any thread.
///
/// \parblock
/// @note  Safe to call from any thread.
/// \endparblock
///
/// \parblock
/// @note  An argument whose page is gone, or one from another page (unless both pages are
///        frames of the same top-level page with exactly the same origin), is delivered as
///        `undefined` and logs a diagnostic.
/// \endparblock
///
ULExport void ulJSAPIEmitEvent(ULJSAPI api, const char* event_path,
                               ULJSEventArgsCallback args_callback, void* user_data,
                               ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Promises
 *****************************************************************************/

///
/// Create a pending Promise and the resolver that settles it.
///
/// The promise is an ordinary value: pass it to page script (eg, return it from a bound function)
/// and settle it later through the resolver. Only the first resolve, reject, or complete counts.
///
/// @param  ctx       The context to create the promise in.
///
/// @param  resolver  Receives the resolver. You must call ulDestroyJSPromiseResolver() when
///                   finished with it.
///
/// @return  Returns the promise as a new ULJSValue handle (NULL if the context is gone or
///          `resolver` is NULL, and then nothing is stored). You must call ulDestroyJSValue()
///          when finished.
///
ULExport ULJSValue ulCreateJSPromise(ULJSContext ctx, ULJSPromiseResolver* resolver);

///
/// Resolve the promise with a value.
///
/// Nothing happens if the promise was already settled or the promise's page is gone.
///
/// @param  resolver  The resolver.
///
/// @param  value     The value (it remains yours; NULL for `undefined`).
///
/// \parblock
/// @note  Must be called on the Renderer's thread. From another thread, use
///        ulJSPromiseResolverComplete().
/// \endparblock
///
/// \parblock
/// @note  A `value` whose page is gone resolves the promise with `undefined`. A `value` from
///        another page (unless both pages are frames of the same top-level page with exactly
///        the same origin) rejects the promise instead, with a TypeError whose `code` is
///        `"ULJS_CROSS_CONTEXT"`.
/// \endparblock
///
ULExport void ulJSPromiseResolverResolve(ULJSPromiseResolver resolver, ULJSValue value);

///
/// Reject the promise with a reason (see ulCreateJSError()).
///
/// Nothing happens if the promise was already settled or the promise's page is gone.
///
/// @param  resolver  The resolver.
///
/// @param  reason    The rejection reason (it remains yours).
///
/// \parblock
/// @note  Must be called on the Renderer's thread. From another thread, use
///        ulJSPromiseResolverComplete().
/// \endparblock
///
/// \parblock
/// @note  A `reason` whose page is gone rejects the promise with `undefined`. A `reason` from
///        another page (unless both pages are frames of the same top-level page with exactly
///        the same origin) is replaced by a TypeError whose `code` is `"ULJS_CROSS_CONTEXT"`.
/// \endparblock
///
ULExport void ulJSPromiseResolverReject(ULJSPromiseResolver resolver, ULJSValue reason);

///
/// Settle the promise with a value that's created later on the Renderer's thread.
///
/// Call this from any thread. `callback` runs during a later ulUpdate() with a live context to
/// create the value in (values can only be created on the Renderer's thread). If the page goes
/// away first, `callback` is skipped.
///
/// @param  resolver           The resolver.
///
/// @param  callback           The callback that creates the value (see ULJSCompleteCallback).
///
/// @param  user_data          Opaque user data passed to `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after
///                            `callback` runs or is skipped (may be NULL).
///
/// \parblock
/// @note  Safe to call from any thread.
/// \endparblock
///
/// \parblock
/// @note  A value `callback` returns whose page is gone resolves the promise with `undefined`
///        (one it throws rejects the promise with `undefined`). One from another page (unless
///        both pages are frames of the same top-level page with exactly the same origin)
///        rejects the promise with a TypeError whose `code` is `"ULJS_CROSS_CONTEXT"`.
/// \endparblock
///
ULExport void ulJSPromiseResolverComplete(ULJSPromiseResolver resolver,
                                          ULJSCompleteCallback callback, void* user_data,
                                          ULUserDataDestroyCallback destroy_user_data);

///
/// Destroy a resolver (NULL-safe).
///
/// Destroying a resolver that hasn't settled its promise rejects it with an Error, so the page
/// never waits forever.
///
/// @param  resolver  The resolver to destroy.
///
/// @warning  Safe to call from any thread, but destroying must be the resolver's last use. Don't
///           destroy it on one thread while another may still be settling it.
///
ULExport void ulDestroyJSPromiseResolver(ULJSPromiseResolver resolver);

///
/// Whether or not the API whose binding created this resolver was destroyed.
///
/// The resolver an async function callback receives (see ulJSAPIBindAsyncFunction()) becomes
/// detached when the API's last owning handle is destroyed (unbinding or rebinding the path
/// doesn't detach it). Settling it still works, but long-running work can check this to stop
/// early.
///
/// @param  resolver  The resolver.
///
/// @return  Returns true if the API that created the resolver was destroyed. Always false for a
///          resolver from ulCreateJSPromise().
///
/// @note  Safe to call from any thread.
///
ULExport bool ulJSPromiseResolverIsDetached(ULJSPromiseResolver resolver);

///
/// Wait for a JavaScript promise to settle.
///
/// `callback` runs once on the Renderer's thread during a later ulUpdate(), with the value or
/// the rejection reason. If the page goes away first, it runs with a NULL result, so it always
/// runs. A rejection you wait for this way counts as handled, so the page doesn't report it as
/// unhandled.
///
/// @param  promise            The promise.
///
/// @param  callback           The callback to invoke when the promise settles.
///
/// @param  user_data          Opaque user data passed to `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after
///                            `callback` runs (may be NULL).
///
/// @return  Returns true if `promise` is a Promise and `callback` will run. Otherwise it returns
///          false and `destroy_user_data` runs right away.
///
ULExport bool ulJSPromiseThen(ULJSValue promise, ULJSPromiseSettledCallback callback,
                              void* user_data, ULUserDataDestroyCallback destroy_user_data);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_JSAPI_H
