///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_JSClass.h
///
/// Native class definitions and instance wrappers for JavaScript.
///
/// `#include <Ultralight/CAPI/CAPI_JSClass.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// A ULJSClass defines a JavaScript class backed by native C data and callbacks. Page script can
/// instantiate the class with `new` and access its prototype methods and properties, while native
/// code can wrap existing instances for JavaScript to use.
///
/// Define a class with a constructor, a destructor, and a property, then register it on an API:
///
/// ```c
/// #include <stdlib.h>
///
/// typedef struct {
///   double timeout;
/// } Database;
///
/// /* new app.Database() */
/// static void* DatabaseNew(void* user_data, ULJSContext ctx,
///                          const ULJSValue* args, size_t argc,
///                          void** out_holder,
///                          ULUserDataDestroyCallback* out_destroy_holder,
///                          ULJSValue* exception) {
///   Database* db = (Database*)calloc(1, sizeof(Database));
///   db->timeout = 30;
///   return db;  /* the wrapper owns it */
/// }
///
/// static void DatabaseFree(void* user_data, void* instance) {
///   free(instance);
/// }
///
/// /* db.timeout */
/// static ULJSValue DatabaseGetTimeout(void* user_data, ULJSContext ctx,
///                                     void* instance, ULJSValue* exception) {
///   return ulCreateJSValueNumber(ctx, ((Database*)instance)->timeout);
/// }
///
/// static ULJSClass g_database_class = NULL;
///
/// void RegisterDatabase(ULJSAPI api) {
///   g_database_class = ulCreateJSClass("Database");
///   ulJSClassSetConstructor(g_database_class, DatabaseNew, NULL, NULL);
///   ulJSClassSetDestructor(g_database_class, DatabaseFree, NULL, NULL);
///   ulJSClassAddProperty(g_database_class, "timeout", DatabaseGetTimeout, NULL,
///                        NULL, NULL);
///   ulJSAPIRegisterClass(api, "Database", g_database_class);
/// }
/// ```
///
/// Page script creates an instance and reads its property:
///
/// ```js
/// const db = new app.Database();
/// db.timeout;  // 30
/// ```
///
/// ## Defining a Class
///
/// Method and property callbacks registered on a definition receive the native instance pointer
/// directly as a `void*` argument, while static methods receive no instance.
///
/// You can modify a definition only before its first use. First use happens when you register the
/// class on a ULJSAPI with ulJSAPIRegisterClass() or wrap an instance with
/// ulCreateJSObjectWithClass() or ulCreateJSObjectWithClassHolder().
///
/// You'll need to keep the ULJSClass handle for wrapping native instances, detaching wrappers, and
/// looking up instance pointers.
///
/// ## Instance Ownership
///
/// Each native instance has at most one JavaScript wrapper for a class in any single context.
/// Passing the same instance pointer to ulCreateJSObjectWithClass() or
/// ulCreateJSObjectWithClassHolder() for that class in the same context returns that existing
/// wrapper, preserving JavaScript object identity (`===`).
///
/// Instance ownership depends on how the wrapper is created:
///
/// - **Instances created with `new` belong to their wrapper.** Your constructor callback allocates
///   the native instance, and the wrapper takes ownership unless you store a custom holder in
///   `out_holder`.
/// - **Passing `true` for `adopt` in ulCreateJSObjectWithClass() transfers ownership to the
///   wrapper.** Use this when native code creates an instance that JavaScript should manage.
/// - **Passing `false` for `adopt` in ulCreateJSObjectWithClass() borrows the instance.** Native
///   code retains ownership and must keep the instance alive while its page is alive.
/// - **Passing a holder delegates ownership to a custom cleanup callback.** Use
///   ulCreateJSObjectWithClassHolder() when ownership relies on a token that can't be reconstructed
///   from a raw pointer.
///
/// The library destroys owned instances on the Renderer's thread during a later ulUpdate() call,
/// after their wrapper is garbage-collected or when the page goes away. If you don't set a
/// destructor with ulJSClassSetDestructor(), owned instances without a holder are never destroyed.
///
/// @warning  Never adopt the same instance pointer into two owning wrappers (such as across two
///           contexts or two classes), because the library destroys it twice.
///
/// ## Detaching an Instance
///
/// Garbage collection runs at unpredictable times, so any class that manages a scarce native
/// resource (such as an open file or network socket) should provide an explicit close method.
/// Calling ulJSObjectDetachInstance() releases the native instance from its wrapper and returns the
/// instance pointer. Later script calls to that wrapper's methods or properties throw a TypeError
/// with code `ULJS_DETACHED`.
///
/// Detach an instance inside a close method to free native resources early:
///
/// ```c
/// #include <stdio.h>
///
/// static ULJSClass g_log_class = NULL;
///
/// /* The class destructor (ulJSClassSetDestructor()). */
/// static void LogFileDestroy(void* user_data, void* instance) {
///   fclose((FILE*)instance);
/// }
///
/// /* logFile.close() */
/// static ULJSValue LogFileClose(void* user_data, ULJSContext ctx,
///                               void* instance, ULJSValue this_value,
///                               const ULJSValue* args, size_t argc,
///                               ULJSValue* exception) {
///   void* file = ulJSObjectDetachInstance(this_value, g_log_class, NULL);
///   if (file)
///     LogFileDestroy(NULL, file);  /* the destructor won't run for it now */
///   return NULL;
/// }
/// ```
///
/// Detach a non-owning wrapper with ulJSObjectDetachInstance() before freeing its native instance.
/// Because each page has its own wrapper, detach the wrapper across every context the instance was
/// passed to so script calls throw `ULJS_DETACHED` instead of accessing freed memory.
///
/// When native code continues using an instance after a callback returns (such as for asynchronous
/// work), call ulJSObjectProtectInstance() to obtain a ULJSProtectedInstance token that keeps the
/// native instance alive. While protected, the instance won't be destroyed even if its wrapper is
/// garbage-collected, its page goes away, or ulJSObjectDetachInstance() detaches it. Once the
/// native work finishes, pass the token to ulJSObjectUnprotectInstance() to allow cleanup during a
/// later ulUpdate().
///
/// \parblock
/// @note  Navigating away releases owned instances even when the page enters the back-forward
///        cache. Calls on those instances after the page is restored throw a TypeError with code
///        `ULJS_DETACHED`.
/// \endparblock
///
/// \parblock
/// @note  For conventions on handle ownership, NULL handles, user data, and threads, see
///        CAPI_JSValue.h.
/// \endparblock
///
/// @see  ulCreateJSClass(), ulJSAPIRegisterClass(), ulCreateJSObjectWithClass(),
///       ulCreateJSObjectWithClassHolder(), ulJSObjectDetachInstance(), ulJSObjectProtectInstance()
///
#ifndef ULTRALIGHT_CAPI_JSCLASS_H
#define ULTRALIGHT_CAPI_JSCLASS_H

#include <Ultralight/CAPI/CAPI_JSValue.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to a native class definition.
///
/// @see  ulCreateJSClass(), ulDestroyJSClass()
///
#ifndef ULTRALIGHT_ULJSCLASS_DEFINED
#define ULTRALIGHT_ULJSCLASS_DEFINED
typedef struct C_JSClass* ULJSClass;
#endif

///
/// Opaque token that keeps a native instance alive (see ulJSObjectProtectInstance()).
///
/// @see  ulJSObjectProtectInstance(), ulJSObjectUnprotectInstance()
///
typedef struct C_JSProtectedInstance* ULJSProtectedInstance;

///
/// Callback invoked when page script constructs an instance with `new`.
///
/// Return a new native instance. Its wrapper owns it and destroys it with the class destructor,
/// unless you store a holder in `out_holder`.
///
/// @param  user_data           The user data supplied to ulJSClassSetConstructor().
///
/// @param  ctx                 The context the constructor belongs to. Owned by the library,
///                             valid only for the duration of the callback.
///
/// @param  args                The constructor arguments. Owned by the library, valid only
///                             for the duration of the callback.
///
/// @param  argc                The number of entries in `args`.
///
/// @param  out_holder          Store a holder here to have the wrapper own the instance through
///                             it instead of the class destructor (as with
///                             ulCreateJSObjectWithClassHolder()). Leave it untouched to use the
///                             class destructor.
///
/// @param  out_destroy_holder  The callback that destroys `out_holder`. You must store one
///                             whenever you store a holder.
///
/// @param  exception           To fail construction with a specific error, set `*exception` to
///                             a handle you own (see ulCreateJSError()); ownership transfers to
///                             the library.
///
/// @return  Return the new instance, or NULL to fail construction (page script then gets a
///          generic Error unless you set `*exception`). When construction fails, the library
///          destroys the instance you returned and the holder you stored, if any.
///
/// @note  If you return an instance whose wrapper already owns it, `new` returns that wrapper
///        and the library destroys the holder you stored. If that holder is the instance's only
///        owner, this destroys the instance too.
///
typedef void* (*ULJSConstructorCallback)(void* user_data, ULJSContext ctx,
                                         const ULJSValue* args, size_t argc, void** out_holder,
                                         ULUserDataDestroyCallback* out_destroy_holder,
                                         ULJSValue* exception);

///
/// Callback invoked to destroy an instance owned by its wrapper.
///
/// Runs once per owning wrapper, on the Renderer's thread during a later ulUpdate(), after the
/// wrapper is garbage-collected or its page goes away. Destructions still pending when the
/// Renderer is destroyed run during its destruction.
///
/// @param  user_data  The user data supplied to ulJSClassSetDestructor().
///
/// @param  instance   The instance to destroy.
///
/// @warning  Don't call into JavaScript from this callback.
///
typedef void (*ULJSDestructorCallback)(void* user_data, void* instance);

///
/// Callback invoked when page script calls a method on an instance.
///
/// @param  user_data   The user data supplied to ulJSClassAddMethod().
///
/// @param  ctx         The context the wrapper belongs to. Owned by the library, valid only for
///                     the duration of the callback.
///
/// @param  instance    The native instance (this is never NULL).
///
/// @param  this_value  The instance's wrapper. Owned by the library, valid only for the duration
///                     of the callback.
///
/// @param  args        The call arguments. Owned by the library, valid only for the duration of
///                     the callback.
///
/// @param  argc        The number of entries in `args`.
///
/// @param  exception   To throw an exception into JavaScript, set `*exception` to a handle you
///                     own; ownership transfers to the library and the return value is
///                     ignored.
///
/// @return  Return the method's return value as a handle you own (ownership transfers to the
///          library), or NULL for `undefined`.
///
/// @note  A call on a detached wrapper, or on an object that isn't an instance of the class,
///        throws a TypeError instead of invoking the callback.
///
typedef ULJSValue (*ULJSMethodCallback)(void* user_data, ULJSContext ctx, void* instance,
                                        ULJSValue this_value, const ULJSValue* args,
                                        size_t argc, ULJSValue* exception);

///
/// Callback invoked when page script reads an accessor property of an instance.
///
/// @param  user_data  The user data supplied to ulJSClassAddProperty().
///
/// @param  ctx        The context the wrapper belongs to. Owned by the library, valid only for
///                    the duration of the callback.
///
/// @param  instance   The native instance (this is never NULL).
///
/// @param  exception  To throw an exception into JavaScript, set `*exception` to a handle you
///                    own; ownership transfers to the library and the return value is ignored.
///
/// @return  Return the property's value as a handle you own (ownership transfers to the
///          library), or NULL for `undefined`.
///
typedef ULJSValue (*ULJSPropertyGetterCallback)(void* user_data, ULJSContext ctx,
                                                void* instance, ULJSValue* exception);

///
/// Callback invoked when page script assigns an accessor property of an instance.
///
/// @param  user_data  The user data supplied to ulJSClassAddProperty().
///
/// @param  ctx        The context the wrapper belongs to. Owned by the library, valid only for
///                    the duration of the callback.
///
/// @param  instance   The native instance (this is never NULL).
///
/// @param  value      The assigned value. Owned by the library, valid only for the duration of
///                    the callback.
///
/// @param  exception  To throw an exception into JavaScript, set `*exception` to a handle you
///                    own; ownership transfers to the library and the return value is ignored.
///
/// @return  Return true to accept the assignment. Return false to reject it, which throws a
///          TypeError into the page (unless you set `*exception`).
///
typedef bool (*ULJSPropertySetterCallback)(void* user_data, ULJSContext ctx, void* instance,
                                           ULJSValue value, ULJSValue* exception);

/******************************************************************************
 * Class definition
 *****************************************************************************/

///
/// Create a new, empty class definition.
///
/// You can change the definition until its first use (registering it with a JavaScript API or
/// wrapping an instance with it). After that, the Set and Add functions log a warning, ignore
/// the call, and return false.
///
/// @param  name  The class name as a null-terminated UTF-8 string. It appears as the
///               constructor's `name` and in Object.prototype.toString() output.
///
/// @return  Returns a new ULJSClass, or NULL if `name` is NULL or empty. You must call
///          ulDestroyJSClass() when finished.
///
ULExport ULJSClass ulCreateJSClass(const char* name);

///
/// Destroy a class handle (NULL-safe).
///
/// The class definition itself stays alive while a JavaScript API has it registered or any of its
/// wrappers exist.
///
/// @param  cls  The class definition.
///
ULExport void ulDestroyJSClass(ULJSClass cls);

///
/// Set the constructor invoked by `new`.
///
/// Without a constructor, `new` throws a TypeError (code `ULJS_NO_CTOR`). The class still works
/// for instanceof checks, static methods, and instances you wrap with
/// ulCreateJSObjectWithClass().
///
/// @param  cls                The class definition.
///
/// @param  callback           The constructor callback.
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` when the class
///                            definition is destroyed, when the constructor is replaced, or right
///                            away if the call is ignored (may be NULL). It may run on any thread.
///
/// @return  Returns true if the definition changed (false if `cls` is NULL or already in use).
///
ULExport bool ulJSClassSetConstructor(ULJSClass cls, ULJSConstructorCallback callback,
                                      void* user_data,
                                      ULUserDataDestroyCallback destroy_user_data);

///
/// Set the destructor that destroys instances owned by their wrappers.
///
/// Instances owned through a holder (see ulCreateJSObjectWithClassHolder()) use their holder's
/// destroy callback instead. Without a destructor, owned instances are never destroyed.
///
/// @param  cls                The class definition.
///
/// @param  callback           The destructor callback.
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` when the class
///                            definition is destroyed, when the destructor is replaced, or right
///                            away if the call is ignored (may be NULL). It may run on any thread.
///
/// @return  Returns true if the definition changed (false if `cls` is NULL or already in use).
///
ULExport bool ulJSClassSetDestructor(ULJSClass cls, ULJSDestructorCallback callback,
                                     void* user_data,
                                     ULUserDataDestroyCallback destroy_user_data);

///
/// Add an instance method.
///
/// The method is a function on the class prototype, shared by all instances. Adding a name that
/// already exists replaces it.
///
/// @param  cls                The class definition.
///
/// @param  name               The method name as a null-terminated UTF-8 string (required).
///
/// @param  callback           The callback to invoke when page script calls the method
///                            (required).
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` when the class
///                            definition is destroyed, when the method is replaced, or right away
///                            if the call is ignored (may be NULL). It may run on any thread.
///
/// @param  attributes         A logically ORed set of ULJSPropertyAttributes flags for the
///                            method on the prototype. Pass kULJSPropertyAttributes_DontEnum for
///                            standard JavaScript class-method behavior.
///
/// @return  Returns true if the definition changed (false if `cls` is NULL or already in use,
///          or a required argument is missing).
///
ULExport bool ulJSClassAddMethod(ULJSClass cls, const char* name, ULJSMethodCallback callback,
                                 void* user_data, ULUserDataDestroyCallback destroy_user_data,
                                 unsigned attributes);

///
/// Add an instance accessor property.
///
/// Reads call the getter and assignments call the setter, so the page always gets live native
/// state. The property is enumerable (it appears in Object.keys() and JSON.stringify() output)
/// and can't be deleted by script. Adding a name that already exists replaces it.
///
/// @param  cls                The class definition.
///
/// @param  name               The property name as a null-terminated UTF-8 string (required).
///
/// @param  getter             The getter callback (required).
///
/// @param  setter             The setter callback, or NULL to make the property read-only
///                            (assignments from script then have no effect).
///
/// @param  user_data          Opaque user data passed to every invocation of `getter` and
///                            `setter`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` when the class
///                            definition is destroyed, when the property is replaced, or right
///                            away if the call is ignored (may be NULL). It may run on any thread.
///
/// @return  Returns true if the definition changed (false if `cls` is NULL or already in use,
///          or a required argument is missing).
///
ULExport bool ulJSClassAddProperty(ULJSClass cls, const char* name,
                                   ULJSPropertyGetterCallback getter,
                                   ULJSPropertySetterCallback setter, void* user_data,
                                   ULUserDataDestroyCallback destroy_user_data);

///
/// Add a static method (a function on the constructor itself, eg, `myApp.Database.open()`).
///
/// A static method receives no instance. Adding a name that already exists replaces it.
///
/// @param  cls                The class definition.
///
/// @param  name               The method name as a null-terminated UTF-8 string (required).
///
/// @param  callback           The callback to invoke when page script calls the method
///                            (required).
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` when the class
///                            definition is destroyed, when the method is replaced, or right away
///                            if the call is ignored (may be NULL). It may run on any thread.
///
/// @return  Returns true if the definition changed (false if `cls` is NULL or already in use,
///          or a required argument is missing).
///
ULExport bool ulJSClassAddStaticMethod(ULJSClass cls, const char* name,
                                       ULJSFunctionCallback callback, void* user_data,
                                       ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Instances
 *****************************************************************************/

///
/// Get the wrapper for a native instance, creating it if needed.
///
/// If the instance already has a wrapper for this class in this context, you get that wrapper.
///
/// @param  ctx       The JavaScript context.
///
/// @param  cls       The class definition.
///
/// @param  instance  The native instance (must not be NULL).
///
/// @param  adopt     Pass true to have the wrapper own the instance (the class destructor then
///                   destroys it). Pass false for a non-owning wrapper. Adopting an instance
///                   whose wrapper is non-owning makes that wrapper owning. Adopting an instance
///                   that is already owned logs a warning and changes nothing.
///
/// @return  Returns a new ULJSValue handle to the wrapper, or NULL if the context is gone or
///          `cls` or `instance` is NULL. You must call ulDestroyJSValue() when finished.
///
/// @note  Keep an instance behind a non-owning wrapper alive until its page goes away, or call
///        ulJSObjectDetachInstance() before you free it.
///
ULExport ULJSValue ulCreateJSObjectWithClass(ULJSContext ctx, ULJSClass cls, void* instance,
                                             bool adopt);

///
/// Get an owning wrapper for a native instance that owns it through a holder, creating the
/// wrapper if needed.
///
/// Use this when ownership is a token you can't rebuild from the instance pointer (eg, a shared
/// smart pointer or a managed-runtime handle). When the wrapper is garbage-collected or its page
/// goes away, `destroy_holder(holder)` runs instead of the class destructor, on the Renderer's
/// thread during a later ulUpdate().
///
/// @param  ctx               The JavaScript context.
///
/// @param  cls               The class definition.
///
/// @param  instance          The native instance (must not be NULL).
///
/// @param  holder            The ownership token (must not be NULL).
///
/// @param  destroy_holder    The callback that releases `holder` (must not be NULL).
///
/// @param  out_holder_taken  Set to true if the wrapper took `holder` (a new wrapper, or a
///                           non-owning wrapper that now owns its instance). Set to false if the
///                           instance's wrapper already owns it, in which case `holder` is still
///                           yours to release. May be NULL.
///
/// @return  Returns a new ULJSValue handle to the wrapper, or NULL if the context is gone or a
///          parameter is NULL (`holder` is then still yours). You must call ulDestroyJSValue()
///          when finished.
///
ULExport ULJSValue ulCreateJSObjectWithClassHolder(ULJSContext ctx, ULJSClass cls,
                                                   void* instance, void* holder,
                                                   ULUserDataDestroyCallback destroy_holder,
                                                   bool* out_holder_taken);

///
/// Get the native instance behind a wrapper.
///
/// @param  object  The wrapper.
///
/// @param  cls     The class definition.
///
/// @return  Returns the instance, or NULL if `object` isn't an attached instance of `cls` or its
///          page is gone.
///
ULExport void* ulJSObjectGetInstance(ULJSValue object, ULJSClass cls);

///
/// How a JavaScript value relates to a class.
///
typedef enum {
  kULJSInstanceState_NotAnInstance = 0,  ///< Not an instance of the class (or its page is gone).
  kULJSInstanceState_Attached,           ///< A live instance (not detached).
  kULJSInstanceState_Detached,           ///< An instance whose wrapper is detached.
} ULJSInstanceState;

///
/// Classify a JavaScript value against a class.
///
/// Use this to tell a detached wrapper (which deserves a "was detached" error) from a value that
/// was never an instance.
///
/// @param  object  The value to classify.
///
/// @param  cls     The class definition.
///
/// @return  Returns the value's state (kULJSInstanceState_NotAnInstance if its page is gone).
///
ULExport ULJSInstanceState ulJSObjectGetInstanceState(ULJSValue object, ULJSClass cls);

///
/// Get the holder a wrapper owns its instance through.
///
/// @param  object  The wrapper.
///
/// @param  cls     The class definition.
///
/// @return  Returns the holder, or NULL if the wrapper doesn't own its instance through a holder
///          (adopted and non-owning wrappers don't), is detached, or isn't an instance of `cls`.
///          The holder stays owned by the wrapper; use ulJSObjectDetachInstance() to take it
///          back.
///
ULExport void* ulJSObjectGetInstanceHolder(ULJSValue object, ULJSClass cls);

///
/// Detach the native instance from its wrapper, taking back ownership.
///
/// Afterward, script calls to the wrapper's methods and accessors throw a TypeError (code
/// `ULJS_DETACHED`), and the class destructor never runs for this wrapper. Wrapping the same
/// instance again creates a new wrapper. Use this to build close methods, and to free an
/// instance that a non-owning wrapper refers to.
///
/// @param  object      The wrapper.
///
/// @param  cls         The class definition.
///
/// @param  out_holder  Receives the holder if the wrapper owned its instance through one (you
///                     then own the holder), or NULL otherwise. Pass NULL to have such a holder
///                     destroyed on the Renderer's thread during a later ulUpdate() instead.
///
/// @return  Returns the instance, or NULL if `object` isn't an attached instance of `cls`.
///
/// \parblock
/// @note  While the instance is protected (see ulJSObjectProtectInstance()), you don't get
///        ownership back: `out_holder` receives NULL, and the wrapper's holder (or the class
///        destructor) releases the instance after the last ulJSObjectUnprotectInstance().
/// \endparblock
///
/// \parblock
/// @warning  If the holder is the instance's only owner, destroying it destroys the instance.
///           Pass NULL for `out_holder` only when you know how the wrapper was created.
/// \endparblock
///
ULExport void* ulJSObjectDetachInstance(ULJSValue object, ULJSClass cls, void** out_holder);

///
/// Keep a wrapper's native instance alive until you unprotect it.
///
/// Use this when native code keeps using an instance after the call that received it returns
/// (eg, a method that finishes its work later). While the instance is protected, the wrapper's
/// ownership isn't released, even if the wrapper is garbage-collected, its page goes away, or it
/// is detached with ulJSObjectDetachInstance(). Those still happen as usual (a detached wrapper
/// still throws), but if one of them would have released the instance, the release waits until
/// the last ulJSObjectUnprotectInstance() and then runs on the Renderer's thread during a later
/// ulUpdate().
///
/// You can protect an instance more than once. Each call returns a token that you must pass to
/// ulJSObjectUnprotectInstance() exactly once.
///
/// This differs from JSValueProtect(), which keeps a JavaScript value from being
/// garbage-collected. Protecting an instance keeps the native instance alive, not the wrapper,
/// and it holds across every way the wrapper can release it.
///
/// @param  object  The wrapper.
///
/// @param  cls     The class definition.
///
/// @return  Returns a token for ulJSObjectUnprotectInstance(), or NULL if `object` isn't an
///          attached instance of `cls` or its page is gone.
///
/// @note  Call this on the Renderer's thread. A non-owning wrapper's instance stays yours to keep
///        alive, protected or not.
///
ULExport ULJSProtectedInstance ulJSObjectProtectInstance(ULJSValue object, ULJSClass cls);

///
/// Stop protecting a native instance.
///
/// If the wrapper's ownership was released while the instance was protected and this was the
/// last protection, the instance is released on the Renderer's thread during a later ulUpdate().
/// The token works after the wrapper's page is gone.
///
/// @param  instance  The token returned by ulJSObjectProtectInstance() (NULL does nothing). It is
///                   no longer valid after this call.
///
/// @note  Safe to call from any thread. While the Renderer is alive, the release runs on the
///        Renderer's thread. After the Renderer is destroyed, it runs on the calling thread.
///
ULExport void ulJSObjectUnprotectInstance(ULJSProtectedInstance instance);

///
/// Tell the garbage collector how much native memory an object keeps alive.
///
/// Use this when a small wrapper keeps a large native allocation alive, so the collector
/// accounts for its true cost. A new hint replaces the previous one, and 0 clears it. The hint
/// goes away when the object is collected.
///
/// @param  object  The object (usually a class instance's wrapper).
///
/// @param  bytes   The number of bytes of native memory the object keeps alive.
///
/// @return  Returns true if the hint was stored, or false if the object can't hold a hint (class
///          instances can, plain objects can't) or its page is gone.
///
ULExport bool ulJSObjectSetExternalMemoryHint(ULJSValue object, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_JSCLASS_H
