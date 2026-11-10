///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMData.h
///
/// Data bindings that connect native C data to HTML page markup.
///
/// `#include <Ultralight/CAPI/CAPI_DOMData.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// The C data-binding API connects native data to HTML markup in an attached View. Your markup
/// declares where values appear and which elements dispatch actions, while native code retains
/// ownership of application state.
///
/// This header is the C mirror of the C++ data-binding API. It uses the same markup attributes and
/// synchronization rules.
///
/// First, describe the native struct using a type builder:
///
/// ```c
/// typedef struct {
///   long long health;
///   double volume;
/// } Player;
///
/// // Each entry's index is its position in the order it's added below.
/// enum { kPlayer_Health, kPlayer_Volume, kPlayer_UsePotion };
///
/// static ULDOMDataType player_type;
///
/// bool DescribePlayer(void) {
///   bool ok = true;
///   ULDOMDataTypeBuilder b = ulCreateDOMDataTypeBuilder("Player");
///   ok &= ulDOMDataTypeBuilderAddField(b, "health", kULDOMDataValueKind_Int64, 0);
///   ok &= ulDOMDataTypeBuilderAddField(b, "volume", kULDOMDataValueKind_Double,
///                                      kULDOMDataEntryFlags_Editable);
///   ok &= ulDOMDataTypeBuilderAddAction(b, "usePotion", NULL, false);
///   player_type = ulDOMDataTypeBuilderBuild(b);  // lives for the program
///   ulDestroyDOMDataTypeBuilder(b);
///   return ok;
/// }
/// ```
///
/// Next, bind an instance to a context attached to a View, and synchronize it once per frame:
///
/// ```c
/// static Player player = {80, 0.8};
/// static ULDOMDataContext ctx;
/// static ULDOMDataBinding binding;
///
/// // Writes the instance's values during each sync.
/// static void WalkPlayer(ULDOMDataRecord record, const void* instance) {
///   const Player* p = (const Player*)instance;
///   ulDOMDataRecordSetInt64(record, kPlayer_Health, p->health);
///   ulDOMDataRecordSetDouble(record, kPlayer_Volume, p->volume);
/// }
///
/// // Returns the instance to read on each sync.
/// static const void* BorrowPlayer(void* holder_state) {
///   return holder_state;
/// }
///
/// bool BindPlayer(ULView view) {
///   ctx = ulCreateDOMDataContext();  // this thread becomes its home thread
///   binding = ulDOMDataContextBind(ctx, "player", player_type, WalkPlayer,
///                                  &player, BorrowPlayer, NULL, NULL);
///   return ulViewAttachDOMDataContext(view, ctx,
///                                     kULDOMDataContextAttachFlags_None, NULL, 0);
/// }
///
/// void Tick(void) {  // once per frame
///   player.health -= 1;
///   ulDOMDataContextSync(ctx);
/// }
/// ```
///
/// In your HTML markup, bind elements to the player's fields and action:
///
/// ```html
/// <p>Health: {{player.health}}</p>
/// <input type="range" min="0" max="1" step="0.01" ul-value="player.volume">
/// <button type="button" ul-on:click="player.usePotion">Drink Potion</button>
/// ```
///
/// ## Describing Types
///
/// Type descriptions define the schema that pages and callbacks use to identify fields and actions:
///
/// - **Track entry indexes with an enum.** Each field or action gets an index matching the order
///   you add it to the builder, which your record functions and callbacks use to identify the
///   entry.
/// - **Check the return value of each addition.** When an add call fails, the library logs a
///   warning and assigns no index, shifting all later indexes down by one.
/// - **Build each type once at startup.** A built ULDOMDataType lives for the lifetime of the
///   program, so you never destroy it.
///
/// ## Writing Values
///
/// During each synchronization, the library runs your ULDOMDataSyncWalkCallback to read the
/// instance:
///
/// - **Write values into the provided ULDOMDataRecord handle.** The record handle and any child
///   records you obtain from it are valid only during the walk callback, so you must never store or
///   destroy them.
/// - **Writing an unchanged value sends nothing to the page.** Any field the walk callback doesn't
///   write keeps its current value on the page.
/// - **Keep the walk callback free of side effects.** The walk callback also runs when exporting
///   schemas through ulDOMDataContextGetSchema() or ulDOMDataContextDumpSchema(), so it must remain
///   a pure read of your data.
///
/// ## Receiving Page Input
///
/// This example registers callbacks on the binding to handle volume edits and potion clicks:
///
/// ```c
/// static void OnChange(void* user_data, unsigned int slot,
///                      const ULDOMDataLeafValue* value) {
///   Player* p = (Player*)user_data;
///   if (slot == kPlayer_Volume && value->kind == kULDOMDataValueKind_Double) {
///     double v = value->double_value;
///     p->volume = v < 0 ? 0 : (v > 1 ? 1 : v);  // store it to accept the edit
///   }
/// }
///
/// static void OnAction(void* user_data, unsigned int slot,
///                      const ULDOMDataActionSource* source,
///                      const ULDOMDataPayloadEntry* payload,
///                      size_t payload_count, ULDOMDataTable staging) {
///   (void)payload;
///   (void)payload_count;
///   (void)staging;
///   Player* p = (Player*)user_data;
///   if (!source && slot == kPlayer_UsePotion)  // NULL source: not a row action
///     p->health = 100;
/// }
///
/// void WatchPlayer(void) {
///   ulDOMDataBindingSetChangeCallback(binding, OnChange, &player, NULL);
///   ulDOMDataBindingSetActionCallback(binding, OnAction, &player, NULL);
/// }
/// ```
///
/// Input from the page follows these rules:
///
/// - **Each binding supports one change callback and one action callback.** Both run inside
///   ulDOMDataContextSync(), with all field edits delivered before any actions.
/// - **Treat incoming edits as requests.** Store the value in your instance to accept the edit, or
///   leave the field unchanged to decline it. Because C bindings lack schema validators, clamp or
///   validate values inside your callback before storing them.
/// - **Input for a binding without a callback is dropped.** Unhandled form edits snap back to the
///   published value after the next sync, and unhandled actions are discarded.
/// - **Inspect `source` to identify row actions.** When an action fires from a row inside a
///   `ul-for` template, `source` is non-`NULL`, identifying the list and row key, while the action
///   index refers to the row type rather than the root type.
///
/// @note  Keep calling ulDOMDataContextSync() while the page is visible (even when the game is
///        paused), because input from the page reaches callbacks only during a sync.
///
/// ## Thread Roles
///
/// A context belongs to the thread that creates it:
///
/// - **Run synchronization and mutations on the home thread.** You bind instances, set callbacks,
///   and call ulDOMDataContextSync() on this thread. Modify bound native data only between sync
///   calls, because ulDOMDataContextSync() reads your data without synchronizing with other
///   threads.
/// - **Several operations are safe to call from any thread.** Each function's documentation states
///   whether you can call it from background threads without dispatching to the home thread.
/// - **Use a thread-safe context when the sync thread varies.** If different worker threads call
///   ulDOMDataContextSync() over time, create the context with ulCreateDOMDataContextThreadSafe().
///   The context serializes its own calls, but you must still synchronize your native data so
///   nothing changes an instance while a thread syncs.
///
/// ## Handles and Lifetime
///
/// Handle ownership and lifetime follow these rules:
///
/// - **Destroy the handles you create and the ULString values this header returns.** You destroy
///   context, binding, and builder handles with their destroy functions, and destroy any ULString
///   returned by getters or schema exports with ulDestroyString() because returned strings belong
///   to the caller. You never destroy built ULDOMDataType descriptors, borrowed ULDOMDataTable
///   views, or walk records.
/// - **Rely on holder destroy callbacks to run exactly once.** If you pass a `destroy_holder`
///   callback to ulDOMDataContextBind(), the library calls it when the binding unbinds, or
///   immediately if ulDOMDataContextBind() fails.
/// - **Expect failed calls to return false or NULL.** Passing a `NULL` handle fails safely and
///   returns quietly, while operational failures (such as a malformed binding name or a failed
///   builder addition) return `false` or `NULL` with a logged warning.
/// - **Call ulDOMDataBindingIsAlive() to check whether a binding still works.** A binding stops
///   working when its name is bound again or its context is destroyed, after which its callbacks
///   are dropped, posted actions fail, and ulDOMDataBindingIsAlive() returns `false`.
///
/// @see  ulCreateDOMDataTypeBuilder(), ulDOMDataContextBind(), ulDOMDataContextSync(),
///       ulViewAttachDOMDataContext(), ulDOMDataBindingSetChangeCallback(),
///       ulDOMDataBindingSetActionCallback(), ulDOMDataContextPostTask()
///
#ifndef ULTRALIGHT_CAPI_DOMDATA_H
#define ULTRALIGHT_CAPI_DOMDATA_H

#include <Ultralight/CAPI/CAPI_DOMDocument.h>
#include <Ultralight/CAPI/CAPI_DOMElement.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// A handle to a data-binding context (the C mirror of dom::data::Context).
///
/// @see  ulCreateDOMDataContext(), ulDestroyDOMDataContext()
///
typedef struct C_DOMDataContext* ULDOMDataContext;

///
/// A handle to one bound instance (the C mirror of dom::data::Binding).
///
/// @see  ulDOMDataContextBind(), ulDestroyDOMDataBinding()
///
typedef struct C_DOMDataBinding* ULDOMDataBinding;

///
/// A read-only view of a bound instance's values (or of one of its nested objects or list rows).
///
/// Read it with the ulDOMDataTable functions. The function or callback that gives you a table
/// says how long it stays valid.
///
/// @see  ulDOMDataContextGetPublishedTable(), ulDOMDataBindingGetStagingTable()
///
typedef const struct C_DOMDataTable* ULDOMDataTable;

///
/// A described type's schema.
///
/// Use it to inspect a schema at runtime (eg, to generate tooling). The table lists the type's
/// entries in schema order, and every function in this header that takes an entry's index uses
/// that order. Identify entries by name, since an entry's index changes when you add or reorder
/// entries.
///
/// A table and everything it points to (names, metadata, child tables) live for the lifetime of
/// the program. Never destroy them.
///
/// @see  ulDOMDataBindingGetType(), ulDOMDataTypeBuilderBuild()
///
typedef const struct C_DOMDataType* ULDOMDataType;

///
/// A handle for writing a bound instance's values during ulDOMDataContextSync() (the C mirror of
/// dom::data::Model).
///
/// Your walk callback gets one for the bound instance as its `record` parameter. Write each
/// field with the ulDOMDataRecord functions, and reach nested objects and list rows through
/// ulDOMDataRecordGetChild() and the list functions. A field you don't write keeps the value the
/// pages already have, and writing a value that hasn't changed sends nothing.
///
/// Writing an entry of another kind (or an out-of-range index) does nothing. Reading one returns
/// false, 0, an empty string, or an unset color.
///
/// \parblock
/// @note  A read returns the value last sent to the pages, including what this walk callback has
///        already written. Compare a field (eg, a revision counter) before you write it.
/// \endparblock
///
/// \parblock
/// @note  A record (and every record you reach from it) is valid only during the walk callback
///        that received it. Don't store it or destroy it.
/// \endparblock
///
typedef struct C_DOMDataRecord* ULDOMDataRecord;

///
/// The kinds of value an entry of a described type holds.
///
/// These match dom::data::ValueKind. Object, list, and action entries hold no value of their
/// own.
///
/// @note  These values stay the same across releases. New kinds are added at the end, so skip
///        an entry whose kind you don't know.
///
typedef enum {
  kULDOMDataValueKind_Bool = 0,    ///< A boolean.
  kULDOMDataValueKind_Int64,       ///< A signed 64-bit integer.
  kULDOMDataValueKind_Double,      ///< A double-precision float.
  kULDOMDataValueKind_String,      ///< UTF-8 text.
  kULDOMDataValueKind_StyleValue,  ///< A CSS number with a unit (see ULDOMStyleUnit).
  kULDOMDataValueKind_Color,       ///< A color (see ULColor).
  kULDOMDataValueKind_Object,      ///< A nested object.
  kULDOMDataValueKind_List,        ///< A list of rows.
  kULDOMDataValueKind_Action,      ///< An action.
} ULDOMDataValueKind;

///
/// One value of a field or of an action's payload.
///
/// Only the member that matches `kind` is meaningful.
///
/// @note  Zero-initialize a value you fill in (eg, `ULDOMDataLeafValue value = { 0 };`) before
///        you set its members, so a member a later release adds reads as 0.
///
typedef struct {
  unsigned char kind;        ///< The kind of value (a ULDOMDataValueKind value).
  bool bool_value;           ///< The value of a boolean.
  long long int64_value;     ///< The value of an integer.
  double double_value;       ///< The value of a double-precision float.
  const char* string_value;  ///< The UTF-8 text of a string. In a value the library gives you,
                             ///< it's never NULL, it's NUL-terminated, and it's valid only during
                             ///< the call. In a value you pass in, NULL means an empty string.
  size_t string_length;      ///< The length of `string_value` in bytes (text can contain NUL
                             ///< characters). The length is exact, so 0 means an empty string.
  ULColor color_value;       ///< The value of a color.
} ULDOMDataLeafValue;

///
/// Called during ulDOMDataContextSync() to write a bound instance's values to the library.
///
/// You supply this when you bind through the C API (the C++ API supplies it for you). Write each
/// field with the ulDOMDataRecord functions. Writing a value that hasn't changed sends nothing to
/// the pages.
///
/// @param  record    The instance's record, for the ulDOMDataRecord functions (valid only during
///                   this call).
///
/// @param  instance  The instance your acquire callback returned.
///
typedef void (*ULDOMDataSyncWalkCallback)(ULDOMDataRecord record, const void* instance);

///
/// Called during ulDOMDataContextSync() to get the bound instance to read.
///
/// @param  holder_state  The holder state you passed to ulDOMDataContextBind().
///
/// @return  Return the instance, or NULL to skip the binding for this sync (pages keep its last
///          values).
///
typedef const void* (*ULDOMDataAcquireCallback)(void* holder_state);

///
/// Called during ulDOMDataContextSync() after the library has read the instance your acquire
/// callback returned (only when it returned one).
///
/// @param  holder_state  The holder state you passed to ulDOMDataContextBind().
///
typedef void (*ULDOMDataReleaseCallback)(void* holder_state);

///
/// Called on the Renderer's thread for a task you posted with ulDOMDataContextPostTask() or
/// ulDOMDataContextPostTaskOnce().
///
/// @param  user_data  The user data you passed when posting the task.
///
/// @param  document   The document of the View this run is for, valid only during the call (use
///                    ulCreateDOMDocumentRef() to keep it). It's NULL when the context has no
///                    attached View, and always NULL for a task posted with
///                    ulDOMDataContextPostTaskOnce().
///
typedef void (*ULDOMDataTaskCallback)(void* user_data, ULDOMDocument document);

///
/// Called inside ulDOMDataContextSync() for an edit of an Editable field (from a `ul-value`
/// control or sent with ulDOMDataContextStageChangeBool() and its siblings).
///
/// The edit is a request. Store the value in your instance to accept it. If you don't, the
/// control shows the published value again after the next ulDOMDataContextSync(). Each field
/// gets at most one call per sync (with the latest value), and all changes come before any
/// action.
///
/// @param  user_data  The user data you passed to ulDOMDataBindingSetChangeCallback().
///
/// @param  slot       The field's index in the bound type's schema.
///
/// @param  value      The new value, valid only during the call.
///
/// @note  `user_data` stays valid until this callback returns, even if you set another callback
///        or destroy the binding's last handle inside it.
///
typedef void (*ULDOMDataChangeCallback)(void* user_data, unsigned int slot,
                                        const ULDOMDataLeafValue* value);

///
/// One field of an action's payload.
///
/// A payload holds the fields of the action's declared payload type, each found by name. A
/// payload declared as a single value (eg, `dd::Action<double>`) has one entry named `value`.
///
typedef struct {
  const char* name;          ///< The field's name (never NULL).
  ULDOMDataLeafValue value;  ///< The field's value (a boolean, integer, double, or string kind).
} ULDOMDataPayloadEntry;

///
/// The row that fired a row action (its list and its key).
///
/// Row actions come only from `ul-on:*` markup inside a `ul-for` row.
///
typedef struct {
  ///
  /// The list's index in the bound type's schema.
  ///
  unsigned int list_slot;

  ///
  /// Whether or not the row key is a string.
  ///
  bool is_string_key;

  ///
  /// The row's integer key when `is_string_key` is false (the row's position for a list
  /// without keys).
  ///
  long long int_key;

  ///
  /// The row's UTF-8 string key when `is_string_key` is true (NUL-terminated). It's valid only
  /// during the call.
  ///
  const char* string_key;

  ///
  /// The length of `string_key` in bytes (0 when the key isn't a string).
  ///
  size_t string_key_length;
} ULDOMDataActionSource;

///
/// Called inside ulDOMDataContextSync() for an action (fired from `ul-on:*` markup or sent with
/// ulDOMDataContextPostAction()).
///
/// Actions come after all changes, in the order they fired. Actions from markup have no
/// payload.
///
/// @param  user_data      The user data you passed to ulDOMDataBindingSetActionCallback().
///
/// @param  slot           The action's index in the bound type's schema (in the row type's
///                        schema for a row action).
///
/// @param  source         The list and row key for a row action, or NULL for an action of the
///                        bound type itself. Valid only during the call.
///
/// @param  payload        The payload's fields, or NULL when there's no payload. Valid only
///                        during the call.
///
/// @param  payload_count  The number of entries in `payload` (0 when there's no payload).
///
/// @param  staging        The binding's table, for reading your instance's values as of the
///                        last sync (list rows included). Valid only during the call.
///
/// @note  `user_data` stays valid until this callback returns, even if you set another callback
///        or destroy the binding's last handle inside it.
///
/// @warning  Destroying the binding's last handle inside this callback destroys `staging` right
///           away, so don't use it afterwards.
///
/// @see  ulDOMDataBindingGetStagingTable()
///
typedef void (*ULDOMDataActionCallback)(void* user_data, unsigned int slot,
                                        const ULDOMDataActionSource* source,
                                        const ULDOMDataPayloadEntry* payload,
                                        size_t payload_count, ULDOMDataTable staging);

///
/// Called on the Renderer's thread to turn a value into the text of `{{path|name}}`.
///
/// @param  user_data  The user data you passed to ulDOMDataContextDefineFormat().
///
/// @param  value      The value (a boolean, integer, double, or string kind), valid only during
///                    the call.
///
/// @param  output     The text to show. It starts empty. Set it with ulStringAssignCString().
///
/// @warning  A formatter must return the same text for the same value and must not call the
///           data-binding API.
///
typedef void (*ULDOMDataFormatCallback)(void* user_data, const ULDOMDataLeafValue* value,
                                        ULString output);

/******************************************************************************
 * Context
 *****************************************************************************/

///
/// Create a data-binding context whose home thread is the calling thread.
///
/// @return  Returns a new context. You must call ulDestroyDOMDataContext() when finished.
///
ULExport ULDOMDataContext ulCreateDOMDataContext(void);

///
/// Create a data-binding context that any thread can use.
///
/// Use this when the thread that calls ulDOMDataContextSync() changes over time (eg, a job system
/// that syncs on different worker threads). Any thread can then bind, sync, and set callbacks.
/// The calls take turns (each one waits until the running one finishes), and change and action
/// callbacks run on the thread that called ulDOMDataContextSync().
///
/// @warning
/// \parblock
/// Only the context's own calls are thread-safe, **not** your bound instances.
/// ulDOMDataContextSync() reads them without any lock you can see, so don't change an instance
/// while another thread may be syncing. To hand an instance to another thread, use your own
/// synchronization.
///
/// A callback that waits on another thread that needs this context deadlocks. Calling back into
/// the context from the callback itself is fine.
/// \endparblock
///
/// @return  Returns a new context. You must call ulDestroyDOMDataContext() when finished.
///
ULExport ULDOMDataContext ulCreateDOMDataContextThreadSafe(void);

///
/// Add an owning reference to a context handle.
///
/// @param  context  The context handle.
///
/// @return  Returns an owning handle to the context (NULL if `context` is NULL). It's the same
///          handle ulCreateDOMDataContext() returned. You must call ulDestroyDOMDataContext()
///          when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMDataContext ulCreateDOMDataContextRef(ULDOMDataContext context);

///
/// Add a reference to a context that doesn't own it.
///
/// A borrowed handle works with every function in this header, but it doesn't keep the context
/// attached: once the last owning handle is destroyed, the context detaches from its Views as
/// ulDestroyDOMDataContext() describes, and the borrowed handle stays safe to use.
///
/// @param  context  The context handle.
///
/// @return  Returns a new borrowed handle (NULL if `context` is NULL). You must call
///          ulDestroyDOMDataContext() when finished. It's a different handle than `context`,
///          so compare the handles you own instead.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMDataContext ulCreateDOMDataContextBorrowedRef(ULDOMDataContext context);

///
/// Destroy a context handle.
///
/// The View keeps the context attached while an owning handle to it exists (one from
/// ulCreateDOMDataContext(), ulCreateDOMDataContextThreadSafe(), or
/// ulCreateDOMDataContextRef()). When you destroy the last one, the context detaches from every
/// View (its pages drop its bindings the same way ulViewDetachDOMDataContext() does), and its
/// bindings and formatters are destroyed. Their destroy callbacks run on the calling thread, so
/// destroy the last owning handle on the context's home thread.
///
/// The memory stays valid while another reference exists (a borrowed handle, a binding, or a
/// task posted with ulDOMDataContextPostTask() that hasn't finished), so those stay safe to use.
///
/// @param  context  The context handle (can be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMDataContext(ULDOMDataContext context);

///
/// Options for ulViewAttachDOMDataContext().
///
typedef enum {
  kULDOMDataContextAttachFlags_None = 0,  ///< No options (none are defined yet).
} ULDOMDataContextAttachFlags;

///
/// Attach a data-binding context to a View.
///
/// Every page the View loads then gets the context's bindings, starting with the current page
/// during a later ulUpdate(). The View keeps the context attached while an owning handle to it
/// exists (see ulDestroyDOMDataContext()). One context can attach to any number of Views, and a
/// View can have several contexts. Bindings apply only to a View's main frame.
///
/// `origin_rules` lists the pages that get the bindings, in the `scheme://host[:port]` form
/// ulViewAttachJSAPI() uses (see that function for the full syntax). With no rules, only your
/// application's own content gets them (local pages and pages loaded with ulViewLoadHTML()). Rules
/// replace that default-- add `file://*` if you still want local pages (pages loaded with
/// ulViewLoadHTML() match no rule, and a data context has no filter that allows them).
///
/// A page whose origin isn't allowed shows its authored markup, and its controls send no input.
/// Attaching an attached context again replaces its flags and rules.
///
/// @param  view              The View (must not be NULL).
///
/// @param  context           The context handle.
///
/// @param  flags             A bitwise OR of ULDOMDataContextAttachFlags values (pass
///                           kULDOMDataContextAttachFlags_None). Unknown bits fail the attach.
///
/// @param  origin_rules      An array of `num_origin_rules` rules, or NULL for the default
///                           rules.
///
/// @param  num_origin_rules  The number of entries in `origin_rules` (0 for the default rules).
///
/// @return  Returns true on success. Returns false if `context` is NULL, `flags` has an unknown
///          bit, `origin_rules` is NULL but `num_origin_rules` isn't 0, a rule fails to parse,
///          or the context's last owning handle was destroyed. Nothing changes then (an earlier
///          attachment stays as it was), and a logged warning says why.
///
/// @note  Safe to call from any thread.
///
/// @see  ulViewDetachDOMDataContext()
///
ULExport bool ulViewAttachDOMDataContext(ULView view, ULDOMDataContext context,
                                         unsigned int flags, const char* const* origin_rules,
                                         size_t num_origin_rules);

///
/// Detach a data-binding context from a View.
///
/// During a later ulUpdate(), the View's page rebuilds its bindings from the contexts still
/// attached. When none are left, the page gets its authored markup back (`{{path}}` text,
/// `ul-if` templates, and no rows). Values already written by `ul-text`, classes, attributes,
/// and styles stay.
///
/// @param  view     The View (must not be NULL).
///
/// @param  context  The context handle.
///
/// @note  Safe to call from any thread.
///
ULExport void ulViewDetachDOMDataContext(ULView view, ULDOMDataContext context);

///
/// Bind an instance of a described type under a binding name.
///
/// Pages reach the instance's fields through the binding name (eg, `hud.health`), and each
/// ulDOMDataContextSync() sends your changes to them. The library reads the instance through the
/// callbacks you pass here (the C++ API supplies them for you).
///
/// Binding a name again replaces the earlier binding. Its handle stops working, and the library
/// destroys its callbacks' user data and its holder state.
///
/// @param  context          The context handle.
///
/// @param  name             The binding name pages use (copied). It starts with a letter or an
///                          underscore, followed by letters, digits, and underscores (eg,
///                          `hud` or `main_menu`).
///
/// @param  type_descriptor  The bound type (from ulDOMDataTypeBuilderBuild(), or one the C++ API
///                          builds). It must stay valid as long as the context exists (both of
///                          those always do).
///
/// @param  walk             Writes the instance's values during each sync.
///
/// @param  holder_state     Your state for the three holder callbacks. To bind an instance that
///                          you keep alive yourself, pass the instance and an `acquire` that
///                          returns it.
///
/// @param  acquire          Returns the instance to read during each sync.
///
/// @param  release          Called after each read of the instance (can be NULL).
///
/// @param  destroy_holder   Destroys `holder_state` when the binding is unbound, or before this
///                          call returns if the bind fails (can be NULL).
///
/// @return  Returns a new binding handle, or NULL if `name` isn't a valid binding name (the
///          library logs a warning), or if `type_descriptor`, `walk`, or `acquire` is NULL. You
///          must call ulDestroyDOMDataBinding() when finished.
///
/// @note  A C++ type compiled against headers from another release is refused with a logged
///        warning (rebuild against the current headers).
///
/// @warning  Destroying the binding's last handle unbinds the instance.
///
ULExport ULDOMDataBinding ulDOMDataContextBind(ULDOMDataContext context, const char* name,
                                               ULDOMDataType type_descriptor,
                                               ULDOMDataSyncWalkCallback walk,
                                               void* holder_state,
                                               ULDOMDataAcquireCallback acquire,
                                               ULDOMDataReleaseCallback release,
                                               ULUserDataDestroyCallback destroy_holder);

///
/// Deliver input from the page to your callbacks and send your changes to the pages.
///
/// Call this once per tick on the home thread. Each call does three things in order:
///
///  1. **Delivers input from the page** to your callbacks (changes first, then actions). Input
///     for a binding without a callback of that kind is dropped.
///  2. **Reads every bound instance** through its walk callback.
///  3. **Sends what changed** to the pages as one update.
///
/// Pages show the update during a later ulUpdate(), and they never show part of one. A sync with
/// nothing to send is cheap.
///
/// @param  context  The context handle.
///
/// \parblock
/// @note  Input from the page reaches your callbacks only here, so keep calling this while the
///        page is visible (even when your game is paused).
/// \endparblock
///
/// \parblock
/// @note  A call from inside one of your callbacks does nothing (the library logs a warning).
/// \endparblock
///
ULExport void ulDOMDataContextSync(ULDOMDataContext context);

///
/// Get the generation of the last update ulDOMDataContextSync() sent to the pages.
///
/// The generation goes up by one with each update.
///
/// @param  context  The context handle.
///
/// @return  Returns the generation (0 before the first update or for a NULL context).
///
ULExport unsigned long long ulDOMDataContextGetGeneration(ULDOMDataContext context);

///
/// Set how many actions can wait for the next ulDOMDataContextSync(). (Default = 1024)
///
/// When the queue is full, new actions are dropped (the library logs a warning). For an action
/// the page fires continuously (eg, a scroll position), declare it only-latest
/// (kULDOMDataEntryFlags_OnlyLatest) so each new one replaces the waiting one instead of raising
/// the limit.
///
/// @param  context   The context handle.
///
/// @param  capacity  The number of actions (values below 1 become 1).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDOMDataContextSetActionQueueCapacity(ULDOMDataContext context,
                                                     unsigned int capacity);

///
/// Get how many actions can wait for the next ulDOMDataContextSync().
///
/// @param  context  The context handle.
///
/// @return  Returns the limit (0 for a NULL context).
///
/// @note  Safe to call from any thread.
///
ULExport unsigned int ulDOMDataContextGetActionQueueCapacity(ULDOMDataContext context);

///
/// Send an edit of an Editable bool field as if the user made it in the page.
///
/// The edit reaches the change callback at the next ulDOMDataContextSync(). Several edits of one
/// field before then arrive as one (with the latest value). Validators don't run on edits sent
/// this way.
///
/// @param  context  The context handle.
///
/// @param  binding  The binding handle (from this context).
///
/// @param  slot     The field's index in the bound type's schema.
///
/// @param  value    The new value.
///
/// @return  Returns true if the edit was queued, or false if the entry isn't an Editable bool
///          field of the bound type or `binding` belongs to another context.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataContextStageChangeBool(ULDOMDataContext context,
                                              ULDOMDataBinding binding, unsigned int slot,
                                              bool value);

///
/// Send an edit of an Editable integer field (see ulDOMDataContextStageChangeBool()).
///
/// @param  context  The context handle.
///
/// @param  binding  The binding handle (from this context).
///
/// @param  slot     The field's index in the bound type's schema.
///
/// @param  value    The new value.
///
/// @return  Returns true if the edit was queued, or false if the entry isn't an Editable integer
///          field of the bound type or `binding` belongs to another context.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataContextStageChangeInt64(ULDOMDataContext context,
                                               ULDOMDataBinding binding, unsigned int slot,
                                               long long value);

///
/// Send an edit of an Editable floating-point field (see ulDOMDataContextStageChangeBool()).
///
/// @param  context  The context handle.
///
/// @param  binding  The binding handle (from this context).
///
/// @param  slot     The field's index in the bound type's schema.
///
/// @param  value    The new value.
///
/// @return  Returns true if the edit was queued, or false if the entry isn't an Editable
///          floating-point field of the bound type or `binding` belongs to another context.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataContextStageChangeDouble(ULDOMDataContext context,
                                                ULDOMDataBinding binding, unsigned int slot,
                                                double value);

///
/// Send an edit of an Editable string field (see ulDOMDataContextStageChangeBool()).
///
/// @param  context  The context handle.
///
/// @param  binding  The binding handle (from this context).
///
/// @param  slot     The field's index in the bound type's schema.
///
/// @param  value    The new UTF-8 text (copied). NULL sends an empty string.
///
/// @param  length   The length of `value` in bytes (the text can contain NUL characters). The
///                  length is exact, so 0 sends an empty string.
///
/// @return  Returns true if the edit was queued, or false if the entry isn't an Editable string
///          field of the bound type or `binding` belongs to another context.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataContextStageChangeString(ULDOMDataContext context,
                                                ULDOMDataBinding binding, unsigned int slot,
                                                const char* value, size_t length);

///
/// Send an edit of an Editable color field (see ulDOMDataContextStageChangeBool()).
///
/// @param  context  The context handle.
///
/// @param  binding  The binding handle (from this context).
///
/// @param  slot     The field's index in the bound type's schema.
///
/// @param  value    The new color (an unset color is sent as unset).
///
/// @return  Returns true if the edit was queued, or false if the entry isn't an Editable color
///          field of the bound type or `binding` belongs to another context.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataContextStageChangeColor(ULDOMDataContext context,
                                               ULDOMDataBinding binding, unsigned int slot,
                                               ULColor value);

///
/// Send an action to the binding's action callback as if the page fired it.
///
/// The action reaches the action callback at the next ulDOMDataContextSync(), after all changes
/// and in the order the actions were sent. For an only-latest action
/// (kULDOMDataEntryFlags_OnlyLatest), a new one replaces the one already waiting (keeping its
/// place in the queue).
///
/// @param  context  The context handle.
///
/// @param  binding  The binding handle (from this context).
///
/// @param  slot     The action's index in the bound type's schema.
///
/// @return  Returns true if the action was queued. Returns false if the entry isn't an action of
///          the bound type, `binding` belongs to another context, the binding stopped working (see
///          ulDOMDataBindingIsAlive()), or the action queue is full.
///
/// @note  Safe to call from any thread.
///
/// @see  ulDOMDataContextSetActionQueueCapacity()
///
ULExport bool ulDOMDataContextPostAction(ULDOMDataContext context, ULDOMDataBinding binding,
                                         unsigned int slot);

///
/// Send an action with a payload to the binding's action callback (see
/// ulDOMDataContextPostAction()).
///
/// The payload's fields are copied, and the action callback receives them. For an only-latest
/// action, the waiting action gets the newest payload.
///
/// @param  context        The context handle.
///
/// @param  binding        The binding handle (from this context).
///
/// @param  slot           The action's index in the bound type's schema.
///
/// @param  payload        The payload's fields (can be NULL when `payload_count` is 0). Each
///                        needs a name and a boolean, integer, double, or string value (a
///                        string is read for exactly its `string_length` bytes).
///
/// @param  payload_count  The number of entries in `payload`.
///
/// @return  Returns true if the action was queued. Returns false if the entry isn't an action of
///          the bound type, `binding` belongs to another context, the binding stopped working (see
///          ulDOMDataBindingIsAlive()), a payload entry is invalid, or the action queue is full.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataContextPostActionWithPayload(ULDOMDataContext context,
                                                    ULDOMDataBinding binding,
                                                    unsigned int slot,
                                                    const ULDOMDataPayloadEntry* payload,
                                                    size_t payload_count);

///
/// Send an action by binding name and action name, as if the page fired it.
///
/// Use this where you have no binding handle at hand (eg, in a DOM listener that handles an
/// interaction the markup can't express). Only actions of the bound type itself can be sent
/// (not a row's actions).
///
/// The action reaches its action callback at the next ulDOMDataContextSync(), in order with the
/// actions from the page. An action with an unknown binding or action name is dropped there,
/// with a warning when DOM diagnostics are on (see ulConfigSetDOMDiagnostics()).
///
/// @param  context        The context handle.
///
/// @param  binding_name   The binding name.
///
/// @param  action_name    The action's name in the bound type's schema.
///
/// @param  payload        The payload's fields, or NULL for none (see
///                        ulDOMDataContextPostActionWithPayload()).
///
/// @param  payload_count  The number of entries in `payload`.
///
/// @return  Returns true if the action was queued, or false if a name is NULL or empty, a payload
///          entry is invalid, or the action queue is full.
///
/// \parblock
/// @note  Safe to call from any thread.
/// \endparblock
///
/// \parblock
/// @note  Each call takes one entry in the action queue, even for an only-latest action (repeats
///        merge only when ulDOMDataContextSync() delivers them).
/// \endparblock
///
ULExport bool ulDOMDataContextPostActionByName(ULDOMDataContext context,
                                               const char* binding_name,
                                               const char* action_name,
                                               const ULDOMDataPayloadEntry* payload,
                                               size_t payload_count);

///
/// Run a callback on the Renderer's thread once the pages show your latest data.
///
/// The callback runs during a later ulUpdate(), after the pages have applied the data from:
///
///  - **This sync**, when you post from a callback inside ulDOMDataContextSync().
///  - **The last sync**, when you post from anywhere else.
///
/// It runs once for each attached View and gets that View's document, so it can read the page
/// your data produced. With no attached Views, it runs once with a NULL document. A View whose
/// page is still loading holds it up until that page shows the data. A page that doesn't use
/// this context (eg, its origin isn't allowed) doesn't hold it up.
///
/// @param  context            The context handle.
///
/// @param  task               The callback to run.
///
/// @param  user_data          User data to pass to `task`.
///
/// @param  destroy_user_data  Called once after the last run of `task` (can be NULL). If no
///                            Renderer exists, or the Renderer shuts down first, it's called
///                            without `task` running.
///
/// @note  Safe to call from any thread.
///
/// @see  ulDOMDataContextPostTaskOnce()
///
ULExport void ulDOMDataContextPostTask(ULDOMDataContext context, ULDOMDataTaskCallback task,
                                       void* user_data,
                                       ULUserDataDestroyCallback destroy_user_data);

///
/// Run a callback once on the Renderer's thread after every page shows your latest data.
///
/// Works like ulDOMDataContextPostTask(), but `task` runs once with a NULL document after every
/// attached View has applied the data (when no View is attached, it runs during a later ulUpdate()
/// without waiting for a page). Use it for a signal that must fire once, no matter how many Views
/// show the context (eg, "the UI is ready").
///
/// @param  context            The context handle.
///
/// @param  task               The callback to run.
///
/// @param  user_data          User data to pass to `task`.
///
/// @param  destroy_user_data  Called once after `task` runs (can be NULL). If no Renderer
///                            exists, or the Renderer shuts down first, it's called without
///                            `task` running.
///
/// @note  Safe to call from any thread.
///
ULExport void ulDOMDataContextPostTaskOnce(ULDOMDataContext context,
                                           ULDOMDataTaskCallback task, void* user_data,
                                           ULUserDataDestroyCallback destroy_user_data);

///
/// Run a callback while no other thread uses the context.
///
/// On a context from ulCreateDOMDataContextThreadSafe(), `fn` runs while this thread holds the
/// context, so no other thread's ulDOMDataContextSync() or registration runs at the same time.
/// Calls into this context from inside `fn` are allowed. On a context from
/// ulCreateDOMDataContext(), it just calls `fn`.
///
/// Use it to change state your callbacks read (eg, a table of handlers behind your change and
/// action callbacks) while another thread may be syncing.
///
/// @param  context    The context handle. Nothing runs if it's NULL.
///
/// @param  fn         The function to run (nothing runs if it's NULL).
///
/// @param  user_data  User data to pass to `fn`.
///
/// @note  Safe to call from any thread. On a thread-safe context it waits until no other thread
///        is using the context.
///
ULExport void ulDOMDataContextWithOwnerTurn(ULDOMDataContext context,
                                            void (*fn)(void* user_data), void* user_data);

///
/// Define a formatter for `{{path|name}}` text.
///
/// A formatter turns a bool, number, or string value into the text the page shows:
///
/// ```html
/// <span>{{hud.gold|comma}} gold</span>
/// ```
///
/// Only `{{path}}` text uses formatters (`ul-text` doesn't take one), and only for the bindings of
/// this context. Defining a name again replaces its formatter, and a NULL `callback` removes it
/// (text that uses it then shows the plain value and logs a warning). A new formatter shows the
/// next time the text changes.
///
/// @param  context            The context handle.
///
/// @param  name               The formatter name, used after the pipe. It starts with a letter or
///                            an underscore, followed by letters, digits, and underscores. A
///                            name that doesn't defines nothing (the library logs a warning).
///
/// @param  callback           The formatter, or NULL to remove the formatter with this name.
///
/// @param  user_data          User data to pass to `callback`.
///
/// @param  destroy_user_data  Called once the formatter is replaced or removed, or the context is
///                            destroyed (can be NULL). It can run on the Renderer's thread.
///
/// @warning  Formatters run on the **Renderer's thread** (see ULDOMDataFormatCallback).
///
ULExport void ulDOMDataContextDefineFormat(ULDOMDataContext context, const char* name,
                                           ULDOMDataFormatCallback callback,
                                           void* user_data,
                                           ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Published-side reads (single consumer)
 *****************************************************************************/

///
/// Take the latest update the context sent to the pages, for reading it yourself.
///
/// Use this on a context that isn't attached to any View (eg, to test your bindings). Read
/// the update's tables with ulDOMDataContextGetPublishedTable(). Tables from an earlier call stop
/// being valid.
///
/// @param  context  The context handle.
///
/// @return  Returns the generation of the update you now hold (0 before the first update).
///
/// @note  Only one thread at a time may read a context's published data (this function,
///        ulDOMDataContextGetPublishedTable(), and the tables they return).
///
/// @warning  Don't call this on a context attached to a View. The Renderer reads the published
///           data of an attached context.
///
ULExport unsigned long long ulDOMDataContextAcquirePublished(ULDOMDataContext context);

///
/// Get the generation of the latest update without taking it.
///
/// @param  context  The context handle.
///
/// @return  Returns the generation (0 before the first update or for a NULL context).
///
/// @note  Safe to call from any thread.
///
ULExport unsigned long long
ulDOMDataContextGetPublishedGeneration(ULDOMDataContext context);

///
/// Get a bound instance's table from the update you took with
/// ulDOMDataContextAcquirePublished().
///
/// @param  context  The context handle.
///
/// @param  name     The binding name.
///
/// @return  Returns the table, or NULL if you haven't taken an update or it has no binding with
///          this name. It's valid until the next ulDOMDataContextAcquirePublished() on this
///          context.
///
ULExport ULDOMDataTable ulDOMDataContextGetPublishedTable(ULDOMDataContext context,
                                                          const char* name);

///
/// Read a bool field from a table.
///
/// @param  table  The table (can be NULL).
///
/// @param  slot   The field's index in the table's schema.
///
/// @return  Returns the value, or false if the entry isn't a bool field.
///
ULExport bool ulDOMDataTableGetBool(ULDOMDataTable table, unsigned int slot);

///
/// Read an integer field from a table.
///
/// @param  table  The table (can be NULL).
///
/// @param  slot   The field's index in the table's schema.
///
/// @return  Returns the value, or 0 if the entry isn't an integer field.
///
ULExport long long ulDOMDataTableGetInt64(ULDOMDataTable table, unsigned int slot);

///
/// Read a floating-point field from a table.
///
/// @param  table  The table (can be NULL).
///
/// @param  slot   The field's index in the table's schema.
///
/// @return  Returns the value, or 0 if the entry isn't a floating-point field.
///
ULExport double ulDOMDataTableGetDouble(ULDOMDataTable table, unsigned int slot);

///
/// Read a string field from a table.
///
/// @param  table  The table (can be NULL).
///
/// @param  slot   The field's index in the table's schema.
///
/// @return  Returns a copy of the text (empty if the entry isn't a string field). You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMDataTableGetString(ULDOMDataTable table, unsigned int slot);

///
/// Read a style-value field from a table.
///
/// @param  table      The table (can be NULL).
///
/// @param  slot       The field's index in the table's schema.
///
/// @param  out_value  Receives the number (0 if the entry isn't a style-value field). Can be
///                    NULL.
///
/// @param  out_unit   Receives the unit as a ULDOMStyleUnit value (kULDOMStyleUnit_Number if the
///                    entry isn't a style-value field). Can be NULL.
///
ULExport void ulDOMDataTableGetStyle(ULDOMDataTable table, unsigned int slot,
                                     double* out_value, unsigned int* out_unit);

///
/// Read a color field from a table.
///
/// @param  table      The table (can be NULL).
///
/// @param  slot       The field's index in the table's schema.
///
/// @param  out_color  Receives the color (unset if the entry isn't a color field). Can be NULL.
///
ULExport void ulDOMDataTableGetColor(ULDOMDataTable table, unsigned int slot,
                                     ULColor* out_color);

///
/// Whether or not a nullable object is present in a table.
///
/// @param  table  The table (can be NULL).
///
/// @param  slot   The object's index in the table's schema.
///
/// @return  Returns true if the object is present, or false if it's absent or the entry isn't a
///          nullable object.
///
ULExport bool ulDOMDataTableIsPresent(ULDOMDataTable table, unsigned int slot);

///
/// Get the table of a nested object.
///
/// @param  table  The table (can be NULL).
///
/// @param  slot   The object's index in the table's schema.
///
/// @return  Returns the object's table (valid as long as `table` is), or NULL if the entry isn't
///          an object or has never had a value.
///
/// @note  A nullable object keeps its last table after it becomes absent, so check
///        ulDOMDataTableIsPresent() first.
///
ULExport ULDOMDataTable ulDOMDataTableGetChild(ULDOMDataTable table, unsigned int slot);

///
/// Get the number of rows in a list.
///
/// @param  table  The table (can be NULL).
///
/// @param  slot   The list's index in the table's schema.
///
/// @return  Returns the number of rows (0 if the entry isn't a list).
///
ULExport size_t ulDOMDataTableGetListCount(ULDOMDataTable table, unsigned int slot);

///
/// Get the table of a list row.
///
/// @param  table    The table (can be NULL).
///
/// @param  slot     The list's index in the table's schema.
///
/// @param  ordinal  The row's position in the list (in the order the page shows the rows).
///
/// @return  Returns the row's table (valid as long as `table` is), or NULL if the entry isn't a
///          list or `ordinal` is out of range.
///
ULExport ULDOMDataTable ulDOMDataTableGetListRow(ULDOMDataTable table, unsigned int slot,
                                                 size_t ordinal);

///
/// Get the integer key of a list row.
///
/// @param  table    The table (can be NULL).
///
/// @param  slot     The list's index in the table's schema.
///
/// @param  ordinal  The row's position in the list.
///
/// @return  Returns the row's key (its position for a list without keys), or 0 if the list has
///          string keys, the entry isn't a list, or `ordinal` is out of range.
///
ULExport long long ulDOMDataTableGetListRowKeyInt64(ULDOMDataTable table, unsigned int slot,
                                                    size_t ordinal);

///
/// Get the string key of a list row.
///
/// @param  table    The table (can be NULL).
///
/// @param  slot     The list's index in the table's schema.
///
/// @param  ordinal  The row's position in the list.
///
/// @return  Returns a copy of the row's key (empty if the list doesn't have string keys, the entry
///          isn't a list, or `ordinal` is out of range). You must call ulDestroyString() when
///          finished.
///
ULExport ULString ulDOMDataTableGetListRowKeyString(ULDOMDataTable table, unsigned int slot,
                                                    size_t ordinal);

/******************************************************************************
 * Binding
 *****************************************************************************/

///
/// Add a reference to a binding handle.
///
/// @param  binding  The binding handle.
///
/// @return  Returns `binding` (NULL if `binding` is NULL). You must call ulDestroyDOMDataBinding()
///          when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMDataBinding ulCreateDOMDataBindingRef(ULDOMDataBinding binding);

///
/// Destroy a binding handle.
///
/// Destroying the last handle unbinds the instance and destroys its callbacks' user data and its
/// holder state:
///
///  - **On the home thread** it happens at once (pages see the change after the next
///    ulDOMDataContextSync()). Inside a change or action callback, the user data and holder
///    state are destroyed after that callback returns.
///  - **From another thread or inside a walk callback** it happens at the start of the next
///    ulDOMDataContextSync(), and the binding gets no input at that sync.
///
/// On a context from ulCreateDOMDataContextThreadSafe(), it happens at once only inside one of
/// that context's change or action callbacks. A binding handle keeps its context handle safe to
/// use, but it doesn't keep the context attached (see ulDestroyDOMDataContext()).
///
/// @param  binding  The binding handle (can be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMDataBinding(ULDOMDataBinding binding);

///
/// Get a table of your instance's values as the last ulDOMDataContextSync() read them.
///
/// Use it in a change or action callback to read the values the pages have (list rows
/// included). Read it with the ulDOMDataTable functions.
///
/// @param  binding  The binding handle.
///
/// @return  Returns the table, or NULL if `binding` is NULL or its name was bound again. The
///          table stays valid until the binding is unbound, but a list row's table is valid only
///          until the next ulDOMDataContextSync().
///
ULExport ULDOMDataTable ulDOMDataBindingGetStagingTable(ULDOMDataBinding binding);

///
/// Get the context a binding belongs to.
///
/// @param  binding  The binding handle.
///
/// @return  Returns a new non-owning handle to the context (NULL if `binding` is NULL), like one
///          from ulCreateDOMDataContextBorrowedRef(). You must destroy it with
///          ulDestroyDOMDataContext(), which never detaches the context or destroys its bindings.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMDataContext ulDOMDataBindingGetContext(ULDOMDataBinding binding);

///
/// Get the binding name.
///
/// @param  binding  The binding handle.
///
/// @return  Returns a copy of the binding name (empty if `binding` is NULL or its name was bound
///          again). You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMDataBindingGetName(ULDOMDataBinding binding);

///
/// Whether or not a binding still works.
///
/// A binding stops working once its name is bound again, it's unbound, or its context is destroyed.
/// Its handle stays safe to use, but its callbacks are dropped and actions posted through it fail.
///
/// @param  binding  The binding handle.
///
/// @return  Returns false if `binding` is NULL or has stopped working.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataBindingIsAlive(ULDOMDataBinding binding);

///
/// Set the callback for edits to the binding's Editable fields.
///
/// The callback runs inside ulDOMDataContextSync() (see ULDOMDataChangeCallback) and replaces
/// any earlier one. With no callback, edits are dropped and their controls show the published
/// value again.
///
/// @param  binding            The binding handle.
///
/// @param  callback           The callback, or NULL to remove it.
///
/// @param  user_data          User data to pass to `callback`.
///
/// @param  destroy_user_data  Called once the library is done with `user_data` (can be NULL). It
///                            runs when you set another callback or the binding is unbound. If
///                            that happens inside a change or action callback, it runs after
///                            that callback returns. It runs before this function returns if
///                            `binding` is NULL or its name was bound again.
///
/// @warning  Don't combine the C callbacks with C++ handlers (dom::data::Binding::OnChange() or
///           dom::data::Binding::OnAction()) on the same binding. Registering a C++ handler
///           replaces both callbacks, and setting a callback breaks the C++ handlers.
///
ULExport void ulDOMDataBindingSetChangeCallback(ULDOMDataBinding binding,
                                                ULDOMDataChangeCallback callback,
                                                void* user_data,
                                                ULUserDataDestroyCallback destroy_user_data);

///
/// Set the callback for the binding's actions.
///
/// The callback runs inside ulDOMDataContextSync() (see ULDOMDataActionCallback) and replaces
/// any earlier one. With no callback, actions are dropped.
///
/// @param  binding            The binding handle.
///
/// @param  callback           The callback, or NULL to remove it.
///
/// @param  user_data          User data to pass to `callback`.
///
/// @param  destroy_user_data  Called once the library is done with `user_data` (can be NULL). It
///                            runs when you set another callback or the binding is unbound. If
///                            that happens inside a change or action callback, it runs after
///                            that callback returns. It runs before this function returns if
///                            `binding` is NULL or its name was bound again.
///
/// @warning  Don't combine the C callbacks with C++ handlers (dom::data::Binding::OnChange() or
///           dom::data::Binding::OnAction()) on the same binding. Registering a C++ handler
///           replaces both callbacks, and setting a callback breaks the C++ handlers.
///
ULExport void ulDOMDataBindingSetActionCallback(ULDOMDataBinding binding,
                                                ULDOMDataActionCallback callback,
                                                void* user_data,
                                                ULUserDataDestroyCallback destroy_user_data);

///
/// Get the binding's change callback and its user data.
///
/// Both receive NULL if `binding` is NULL or its name was bound again.
///
/// @param  binding        The binding handle.
///
/// @param  out_callback   Receives the callback (NULL if none is set). Can be NULL.
///
/// @param  out_user_data  Receives the user data set with the callback. Can be NULL.
///
ULExport void ulDOMDataBindingGetChangeCallback(ULDOMDataBinding binding,
                                                ULDOMDataChangeCallback* out_callback,
                                                void** out_user_data);

///
/// Get the binding's action callback and its user data.
///
/// Both receive NULL if `binding` is NULL or its name was bound again.
///
/// @param  binding        The binding handle.
///
/// @param  out_callback   Receives the callback (NULL if none is set). Can be NULL.
///
/// @param  out_user_data  Receives the user data set with the callback. Can be NULL.
///
ULExport void ulDOMDataBindingGetActionCallback(ULDOMDataBinding binding,
                                                ULDOMDataActionCallback* out_callback,
                                                void** out_user_data);

///
/// Report a delivery that your callback had to drop.
///
/// Call this from an action or change callback (see ULDOMDataActionCallback) when it can't use
/// what it received, such as a payload that doesn't match the type your code expects. The
/// library logs `name` and `reason` as a data-binding warning (when the DOM diagnostics level
/// is Warn or higher, see ulConfigSetDOMDiagnostics()), so the drop doesn't go unnoticed. The
/// C++ handlers (dom::data::Binding::OnAction() and dom::data::Binding::OnChange()) report
/// their own drops.
///
/// @param  context  The data context the delivery came through (can be NULL).
///
/// @param  name     The action's or field's name as a null-terminated UTF-8 string.
///
/// @param  reason   Why it was dropped, as a null-terminated UTF-8 string.
///
/// @note  Safe to call from any thread.
///
ULExport void ulDOMDataContextReportDrop(ULDOMDataContext context, const char* name,
                                         const char* reason);

/******************************************************************************
 * Introspection (descriptor tables)
 *****************************************************************************/

///
/// Flags on a schema entry (see ulDOMDataTypeGetEntryFlags()).
///
/// These match dom::data::EntryFlags.
///
/// @note  These values stay the same across releases. New flags are added at the end.
///
typedef enum {
  kULDOMDataEntryFlags_Editable = 1 << 0,    ///< The page may request changes to this field.
  kULDOMDataEntryFlags_Internal = 1 << 1,    ///< Pages can't reach this entry.
  kULDOMDataEntryFlags_OnlyLatest = 1 << 2,  ///< Only the newest waiting action is delivered.
  kULDOMDataEntryFlags_Keyed = 1 << 3,       ///< A list whose rows have a key.
  kULDOMDataEntryFlags_Var = 1 << 4,         ///< A CSS custom property.
  kULDOMDataEntryFlags_Nullable = 1 << 5,    ///< A nested object that may be absent.

  ///
  /// A double field that holds a `float` value, shown with float precision (eg, `0.8f` shows as
  /// `0.8`). Only ulDOMDataTypeBuilderAddField() takes it, and ulDOMDataTypeGetEntryFlags()
  /// doesn't report it.
  ///
  kULDOMDataEntryFlags_Float = 1 << 6,
} ULDOMDataEntryFlags;

///
/// The key kind of an entry without a key (every entry except a keyed list).
///
enum { kULDOMDataNoKeyKind = 0xFF };

///
/// Get the bound type's descriptor table.
///
/// @param  binding  The binding handle.
///
/// @return  Returns the type, or NULL if `binding` is NULL or its name was bound again.
///
ULExport ULDOMDataType ulDOMDataBindingGetType(ULDOMDataBinding binding);

///
/// Get a type's name.
///
/// @param  type  The type (can be NULL).
///
/// @return  Returns the type's unqualified name, or an empty string if it has none. Don't destroy
///          it.
///
/// @note  Safe to call from any thread.
///
ULExport const char* ulDOMDataTypeGetName(ULDOMDataType type);

///
/// Get the number of entries in a type's schema.
///
/// @param  type  The type (can be NULL).
///
/// @return  Returns the number of entries (0 if `type` is NULL).
///
/// @note  Safe to call from any thread.
///
ULExport unsigned int ulDOMDataTypeGetEntryCount(ULDOMDataType type);

///
/// Get an entry's name.
///
/// @param  type   The type (can be NULL).
///
/// @param  index  The entry's index in the type's schema.
///
/// @return  Returns the name, or NULL if `index` is out of range. Don't destroy it.
///
/// @note  Safe to call from any thread.
///
ULExport const char* ulDOMDataTypeGetEntryName(ULDOMDataType type, unsigned int index);

///
/// Get an entry's kind.
///
/// @param  type   The type (can be NULL).
///
/// @param  index  The entry's index in the type's schema.
///
/// @return  Returns the kind as a ULDOMDataValueKind value (an enum field has the string kind),
///          or 0xFF if `type` is NULL or `index` is out of range.
///
/// @note  Safe to call from any thread.
///
ULExport unsigned int ulDOMDataTypeGetEntryKind(ULDOMDataType type, unsigned int index);

///
/// Get an entry's flags.
///
/// @param  type   The type (can be NULL).
///
/// @param  index  The entry's index in the type's schema.
///
/// @return  Returns the ULDOMDataEntryFlags bits (0 if `index` is out of range).
///
/// @note  Safe to call from any thread.
///
ULExport unsigned int ulDOMDataTypeGetEntryFlags(ULDOMDataType type, unsigned int index);

///
/// Get the key kind of a keyed list.
///
/// @param  type   The type (can be NULL).
///
/// @param  index  The entry's index in the type's schema.
///
/// @return  Returns kULDOMDataValueKind_Int64 or kULDOMDataValueKind_String for a keyed list, or
///          kULDOMDataNoKeyKind for any other entry (or if `index` is out of range).
///
/// @note  Safe to call from any thread.
///
ULExport unsigned int ulDOMDataTypeGetEntryKeyKind(ULDOMDataType type, unsigned int index);

///
/// Get the type of a nested object, a list's rows, or an action's payload.
///
/// A payload declared as a single value has a type with one entry named `value`.
///
/// @param  type   The type (can be NULL).
///
/// @param  index  The entry's index in the type's schema.
///
/// @return  Returns the type, or NULL for any other entry, an action without a payload, or an
///          out-of-range `index`.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMDataType ulDOMDataTypeGetEntryChild(ULDOMDataType type, unsigned int index);

///
/// Get the number of metadata pairs on an entry.
///
/// An enum field (declared with the C++ API or ulDOMDataTypeBuilderAddEnumField()) has one pair
/// with the key `enum`. Its value lists the names separated by commas (eg,
/// `Small,Medium,Large`).
///
/// @param  type   The type (can be NULL).
///
/// @param  index  The entry's index in the type's schema.
///
/// @return  Returns the number of pairs (0 if `index` is out of range).
///
/// @note  Safe to call from any thread.
///
/// @see  ulDOMDataTypeGetEntryMetadata()
///
ULExport unsigned int ulDOMDataTypeGetEntryMetadataCount(ULDOMDataType type,
                                                         unsigned int index);

///
/// Get one metadata pair of an entry.
///
/// @param  type            The type (can be NULL).
///
/// @param  index           The entry's index in the type's schema.
///
/// @param  metadata_index  The pair's index (see ulDOMDataTypeGetEntryMetadataCount()).
///
/// @param  out_key         Receives the key (NULL if out of range). Can be NULL. Don't destroy
///                         it.
///
/// @param  out_value       Receives the value (NULL if out of range). Can be NULL. Don't destroy
///                         it.
///
/// @return  Returns true if `index` and `metadata_index` are in range.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDataTypeGetEntryMetadata(ULDOMDataType type, unsigned int index,
                                            unsigned int metadata_index,
                                            const char** out_key, const char** out_value);

/******************************************************************************
 * Schema JSON
 *****************************************************************************/

///
/// Get a type's schema as JSON, for tooling.
///
/// The JSON has a `format` ("ul-schema"), an `api` ("data"), a `version`, the root type's key in
/// `root`, and a `types` object that lists each type's entries in schema order (with their
/// names, value types, flags, key types, child types, and metadata). Nested types are included.
///
/// A reader should ignore keys and value types it doesn't know. The `version` changes only for a
/// change that would break an existing reader.
///
/// @param  type  The type.
///
/// @return  Returns the JSON text (NULL if `type` is NULL). You must call ulDestroyString() when
///          finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULString ulDOMDataTypeGetSchema(ULDOMDataType type);

///
/// When ulDOMDataContextDumpSchema() writes its file.
///
typedef enum {
  kULDOMDataDumpAt_NextSync = 0,  ///< At the end of the next ulDOMDataContextSync().
  kULDOMDataDumpAt_Now = 1,       ///< Right away.
} ULDOMDataDumpAt;

///
/// Get the schema of every bound instance as JSON, with their current values.
///
/// The JSON matches the ulDOMDataTypeGetSchema() form except that a `models` object replaces
/// `root` (it maps each binding name to its type in `types`), and a `values` object maps each
/// binding name to its current values in the form mock data uses. A list holds at most its
/// first 100 rows, and actions and internal entries have no values.
///
/// The mock data tools read this JSON. Since it holds your app's real data, you should only
/// write it from development builds.
///
/// @param  context  The context handle.
///
/// @return  Returns the JSON text (NULL if `context` is NULL). You must call ulDestroyString() when
///          finished.
///
/// @note  This reads every bound instance through its walk callback, so it isn't free. Call it
///        on the home thread.
///
ULExport ULString ulDOMDataContextGetSchema(ULDOMDataContext context);

///
/// Write the JSON from ulDOMDataContextGetSchema() to a file.
///
/// With kULDOMDataDumpAt_NextSync, the library writes the file at the end of the next
/// ulDOMDataContextSync() on the home thread, so you can call this from any thread (eg, from a
/// debug hotkey). A write that fails then logs a warning with the path.
///
/// With kULDOMDataDumpAt_Now, the library writes the file before this returns. Call it on the
/// home thread.
///
/// @param  context  The context handle.
///
/// @param  path     The file's path as a UTF-8 string (on Windows too). An existing file is
///                  replaced.
///
/// @param  when     When to write the file.
///
/// @return  Returns false if `context` or `path` is NULL or `path` is empty. With
///          kULDOMDataDumpAt_Now, also returns false if the write fails.
///
ULExport bool ulDOMDataContextDumpSchema(ULDOMDataContext context, const char* path,
                                         ULDOMDataDumpAt when);

/******************************************************************************
 * Record (Sync walk write surface)
 *****************************************************************************/

///
/// Read a bool field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @return  Returns the value (false if the entry isn't a bool field).
///
ULExport bool ulDOMDataRecordGetBool(ULDOMDataRecord record, unsigned int slot);

///
/// Read an integer field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @return  Returns the value (0 if the entry isn't an integer field).
///
ULExport long long ulDOMDataRecordGetInt64(ULDOMDataRecord record, unsigned int slot);

///
/// Read a floating-point field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @return  Returns the value (0 if the entry isn't a floating-point field).
///
ULExport double ulDOMDataRecordGetDouble(ULDOMDataRecord record, unsigned int slot);

///
/// Read a string field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @return  Returns a copy of the text (empty if the entry isn't a string field). You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMDataRecordGetString(ULDOMDataRecord record, unsigned int slot);

///
/// Read a style-value field.
///
/// @param  record     The record.
///
/// @param  slot       The field's index in the record's schema.
///
/// @param  out_value  Receives the number (0 if the entry isn't a style-value field). Can be
///                    NULL.
///
/// @param  out_unit   Receives the unit as a ULDOMStyleUnit value (kULDOMStyleUnit_Number if the
///                    entry isn't a style-value field). Can be NULL.
///
ULExport void ulDOMDataRecordGetStyle(ULDOMDataRecord record, unsigned int slot,
                                      double* out_value, unsigned int* out_unit);

///
/// Read a color field.
///
/// @param  record     The record.
///
/// @param  slot       The field's index in the record's schema.
///
/// @param  out_color  Receives the color (unset if the entry isn't a color field). Can be NULL.
///
ULExport void ulDOMDataRecordGetColor(ULDOMDataRecord record, unsigned int slot,
                                      ULColor* out_color);

///
/// Write a bool field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @param  value   The value.
///
ULExport void ulDOMDataRecordSetBool(ULDOMDataRecord record, unsigned int slot, bool value);

///
/// Write an integer field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @param  value   The value.
///
ULExport void ulDOMDataRecordSetInt64(ULDOMDataRecord record, unsigned int slot,
                                      long long value);

///
/// Write a floating-point field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @param  value   The value.
///
ULExport void ulDOMDataRecordSetDouble(ULDOMDataRecord record, unsigned int slot,
                                       double value);

///
/// Write a string field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @param  value   The UTF-8 text (copied). NULL writes an empty string.
///
/// @param  length  The length of `value` in bytes (0 writes an empty string). For a NUL-terminated
///                 string, pass `strlen(value)`.
///
ULExport void ulDOMDataRecordSetString(ULDOMDataRecord record, unsigned int slot,
                                       const char* value, size_t length);

///
/// Write a style-value field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @param  value   The number.
///
/// @param  unit    The unit as a ULDOMStyleUnit value (an unknown unit becomes
///                 kULDOMStyleUnit_Number).
///
ULExport void ulDOMDataRecordSetStyle(ULDOMDataRecord record, unsigned int slot,
                                      double value, unsigned int unit);

///
/// Write a color field.
///
/// @param  record  The record.
///
/// @param  slot    The field's index in the record's schema.
///
/// @param  value   The color.
///
ULExport void ulDOMDataRecordSetColor(ULDOMDataRecord record, unsigned int slot,
                                      ULColor value);

///
/// Set whether or not a nullable object is present.
///
/// A nullable object starts absent. While it's absent, bindings that read through it (eg,
/// `hud.target.name`) show default values, and `ul-if="hud.target"` is false. Write the object's
/// fields through ulDOMDataRecordGetChild().
///
/// @param  record   The record.
///
/// @param  slot     The object's index in the record's schema (a nullable object).
///
/// @param  present  Whether or not the object is present.
///
ULExport void ulDOMDataRecordSetPresent(ULDOMDataRecord record, unsigned int slot,
                                        bool present);

///
/// Get the record of a nested object.
///
/// @param  record  The record.
///
/// @param  slot    The object's index in the record's schema.
///
/// @return  Returns the object's record, or NULL if the entry isn't an object.
///
ULExport ULDOMDataRecord ulDOMDataRecordGetChild(ULDOMDataRecord record, unsigned int slot);

///
/// Start writing a list's rows.
///
/// Get a record for each row in the order the page shows them, write the row's fields, then call
/// ulDOMDataRecordListEnd():
///
/// ```c
/// ulDOMDataRecordListBegin(record, kItems, count);
/// for (size_t i = 0; i < count; i++) {
///   ULDOMDataRecord row = ulDOMDataRecordListRowKeyInt64(record, kItems, items[i].id);
///   ulDOMDataRecordSetString(row, kItemName, items[i].name, strlen(items[i].name));
/// }
/// ulDOMDataRecordListEnd(record, kItems);
/// ```
///
/// For a list without keys, use ulDOMDataRecordListRow(). For a keyed list, use the function
/// that matches its key kind (ulDOMDataRecordListRowKeyInt64() or
/// ulDOMDataRecordListRowKeyString()). A list you don't write keeps its rows.
///
/// @param  record  The record.
///
/// @param  slot    The list's index in the record's schema.
///
/// @param  count   The number of rows you'll write (used only to reserve memory).
///
/// @note  Keys must be unique among the rows. With duplicates, the library logs a warning and
///        the last row wins.
///
ULExport void ulDOMDataRecordListBegin(ULDOMDataRecord record, unsigned int slot,
                                       size_t count);

///
/// Get the record of a row in a list without keys.
///
/// @param  record   The record.
///
/// @param  slot     The list's index in the record's schema.
///
/// @param  ordinal  The row's position in the list (0 for the first row).
///
/// @return  Returns the row's record, or NULL if the entry isn't a list or
///          ulDOMDataRecordListBegin() wasn't called for it during this walk.
///
ULExport ULDOMDataRecord ulDOMDataRecordListRow(ULDOMDataRecord record, unsigned int slot,
                                                size_t ordinal);

///
/// Get the record of a row in a list with integer keys.
///
/// The page shows the rows in the order you get their records.
///
/// @param  record   The record.
///
/// @param  slot     The list's index in the record's schema.
///
/// @param  key      The row's key.
///
/// @return  Returns the row's record, or NULL if the entry isn't a list or
///          ulDOMDataRecordListBegin() wasn't called for it during this walk.
///
ULExport ULDOMDataRecord ulDOMDataRecordListRowKeyInt64(ULDOMDataRecord record,
                                                        unsigned int slot, long long key);

///
/// Get the record of a row in a list with string keys.
///
/// The page shows the rows in the order you get their records.
///
/// @param  record      The record.
///
/// @param  slot        The list's index in the record's schema.
///
/// @param  key         The row's key as UTF-8 text (copied).
///
/// @param  key_length  The length of `key` in bytes (0 gives an empty key). For a NUL-terminated
///                     key, pass `strlen(key)`.
///
/// @return  Returns the row's record, or NULL if the entry isn't a list or
///          ulDOMDataRecordListBegin() wasn't called for it during this walk.
///
ULExport ULDOMDataRecord ulDOMDataRecordListRowKeyString(ULDOMDataRecord record,
                                                         unsigned int slot,
                                                         const char* key,
                                                         size_t key_length);

///
/// Finish writing a list's rows (see ulDOMDataRecordListBegin()).
///
/// Rows you didn't write since ulDOMDataRecordListBegin() are removed. Without this call, the
/// list's rows and their order don't change in the pages.
///
/// @param  record  The record.
///
/// @param  slot    The list's index in the record's schema.
///
ULExport void ulDOMDataRecordListEnd(ULDOMDataRecord record, unsigned int slot);

/******************************************************************************
 * Schema registration (runtime-built types)
 *****************************************************************************/

///
/// A handle for declaring a described type at runtime (the C mirror of dom::data::Schema()).
///
/// Add the type's entries in order (an entry's index is its position), then build the type and
/// bind instances of it with ulDOMDataContextBind():
///
/// ```c
/// ULDOMDataTypeBuilder builder = ulCreateDOMDataTypeBuilder("HUD");
/// ulDOMDataTypeBuilderAddField(builder, "health", kULDOMDataValueKind_Int64, 0);  // entry 0
/// ulDOMDataTypeBuilderAddAction(builder, "respawn", NULL, false);                 // entry 1
/// ULDOMDataType hud_type = ulDOMDataTypeBuilderBuild(builder);
/// ulDestroyDOMDataTypeBuilder(builder);
/// ```
///
/// Your walk callback writes the values of a built type (see ULDOMDataRecord). A built type has
/// no validators, so check edits in your change callback.
///
/// ## Entry Names
///
/// Pages use entry names in paths (eg, `hud.health`). A name starts with a letter or an
/// underscore, followed by letters, digits, and underscores (eg, `maxHealth` or `slot_2`). A
/// var's name is two hyphens followed by lowercase letters, digits, and hyphens (eg,
/// `--max-hp`).
///
/// @see  ulCreateDOMDataTypeBuilder(), ulDestroyDOMDataTypeBuilder()
///
typedef struct C_DOMDataTypeBuilder* ULDOMDataTypeBuilder;

///
/// Create a type builder.
///
/// @param  type_name  The type's name for tooling (copied, can be NULL).
///
/// @return  Returns a new type builder. You must call ulDestroyDOMDataTypeBuilder() when
///          finished.
///
ULExport ULDOMDataTypeBuilder ulCreateDOMDataTypeBuilder(const char* type_name);

///
/// Destroy a type builder (a type it built isn't affected).
///
/// @param  builder  The type builder (can be NULL).
///
ULExport void ulDestroyDOMDataTypeBuilder(ULDOMDataTypeBuilder builder);

///
/// Add a field.
///
/// @param  builder  The type builder.
///
/// @param  name     The field's name (copied).
///
/// @param  kind     The field's kind (kULDOMDataValueKind_Bool through
///                  kULDOMDataValueKind_Color).
///
/// @param  flags    Any of kULDOMDataEntryFlags_Editable, kULDOMDataEntryFlags_Internal, and
///                  kULDOMDataEntryFlags_Float, or 0. A style-value field can't be Editable, and
///                  only a double field can be Float.
///
/// @return  Returns true if the field was added, or false (the library logs a warning) if the
///          name, kind, or flags are invalid, the name is taken, or the type was already built.
///
/// \parblock
/// @note  Page edits come back only for an Editable bool, number, string, or color field of the
///        bound type itself. On a field of a row or a nested object, `ul-value` only shows the
///        value.
/// \endparblock
///
/// \parblock
/// @note  To bind a C `float`, add a Float double field and write it with
///        ulDOMDataRecordSetDouble(). Without the flag, the page shows every digit of the double
///        (eg, `0.800000011920929` for `0.8f`).
/// \endparblock
///
ULExport bool ulDOMDataTypeBuilderAddField(ULDOMDataTypeBuilder builder, const char* name,
                                           unsigned int kind, unsigned int flags);

///
/// Add a string field that holds one of a fixed set of names (the C form of an enum field).
///
/// The field works like a string field: write the current name with ulDOMDataRecordSetString(),
/// and an edit arrives as a string. The names go into the schema as the entry's `enum` metadata
/// (see ulDOMDataTypeGetEntryMetadataCount()), so exported schemas and mock data use them.
///
/// @param  builder     The type builder.
///
/// @param  name        The field's name (copied).
///
/// @param  names       The values the field can hold (each copied). Each must be unique, not
///                     empty, and without commas.
///
/// @param  name_count  The number of entries in `names` (at least 1).
///
/// @param  flags       kULDOMDataEntryFlags_Editable, kULDOMDataEntryFlags_Internal, both, or 0.
///
/// @return  Returns true if the field was added, or false (the library logs a warning) if the
///          name, names, or flags are invalid, the name is taken, or the type was already built.
///
/// @note  The library doesn't check the values you write or the page's edits against `names`.
///        Check an edit's name in your change callback.
///
ULExport bool ulDOMDataTypeBuilderAddEnumField(ULDOMDataTypeBuilder builder, const char* name,
                                               const char* const* names, size_t name_count,
                                               unsigned int flags);

///
/// Add a CSS custom property that follows your data.
///
/// The page needs no markup for it. On the bound type itself, the property is set on the page's
/// root element, so the whole document can use it. On a row type, it's set on each row's root
/// element. A number is written without a unit, a style value with its unit, and a color as hex.
///
/// @param  builder  The type builder.
///
/// @param  name     The property's name, starting with two hyphens (copied).
///
/// @param  kind     The value's kind (usually a number, style-value, or color kind).
///
/// @return  Returns true if the property was added, or false (the library logs a warning) if the
///          name or kind is invalid, the name is taken, or the type was already built.
///
/// @note  A var on a type that's only used as a nested object is never set.
///
ULExport bool ulDOMDataTypeBuilderAddVar(ULDOMDataTypeBuilder builder, const char* name,
                                         unsigned int kind);

///
/// Add a nested object.
///
/// Pages reach its fields with a longer path (eg, `hud.player.name`).
///
/// @param  builder   The type builder.
///
/// @param  name      The object's name (copied).
///
/// @param  child     The object's type (from ulDOMDataTypeBuilderBuild() or a C++ type's
///                   descriptor table). Must not be NULL.
///
/// @param  nullable  Whether or not the object may be absent (see ulDOMDataRecordSetPresent()).
///
/// @return  Returns true if the object was added, or false (the library logs a warning) if
///          `child` is NULL, the name is invalid or taken, or the type was already built.
///
ULExport bool ulDOMDataTypeBuilderAddObject(ULDOMDataTypeBuilder builder, const char* name,
                                            ULDOMDataType child, bool nullable);

///
/// Add a list.
///
/// A keyed list identifies each row by its key, so a row keeps its elements in the page when the
/// list reorders. A list without keys matches rows by position.
///
/// @param  builder   The type builder.
///
/// @param  name      The list's name (copied).
///
/// @param  row_type  The type of each row (from ulDOMDataTypeBuilderBuild() or a C++ type's
///                   descriptor table). Must not be NULL.
///
/// @param  key_kind  kULDOMDataValueKind_Int64 or kULDOMDataValueKind_String for a keyed list, or
///                   kULDOMDataNoKeyKind for a list without keys.
///
/// @return  Returns true if the list was added, or false (the library logs a warning) if
///          `row_type` is NULL, `key_kind` is invalid, the name is invalid or taken, or the type
///          was already built.
///
ULExport bool ulDOMDataTypeBuilderAddList(ULDOMDataTypeBuilder builder, const char* name,
                                          ULDOMDataType row_type, unsigned int key_kind);

///
/// Add an action (a request the page sends to your code).
///
/// @param  builder       The type builder.
///
/// @param  name          The action's name (copied).
///
/// @param  payload_type  The type of the action's payload, or NULL for none. It describes the
///                       payload for tooling (the library doesn't check emitted payloads against
///                       it). Its fields can be bool, integer, double, or string fields. For a
///                       single-value payload, pass a type with one field named `value`.
///
/// @param  only_latest   Whether or not waiting emissions collapse into the newest one (see
///                       kULDOMDataEntryFlags_OnlyLatest).
///
/// @return  Returns true if the action was added, or false (the library logs a warning) if the
///          name is invalid or taken, or the type was already built.
///
ULExport bool ulDOMDataTypeBuilderAddAction(ULDOMDataTypeBuilder builder, const char* name,
                                            ULDOMDataType payload_type, bool only_latest);

///
/// Build the declared type.
///
/// The type lives for the lifetime of the program, so build each type once (eg, at startup).
/// After this call, adding entries fails and building again returns the same type.
///
/// @param  builder  The type builder.
///
/// @return  Returns the type (NULL if `builder` is NULL). Never destroy it.
///
ULExport ULDOMDataType ulDOMDataTypeBuilderBuild(ULDOMDataTypeBuilder builder);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMDATA_H
