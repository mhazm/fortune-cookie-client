/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Kelvin W Sherlock (ksherlock@gmail.com)
 * Copyright (C) 2020 Janrupf (business.janrupf@gmail.com)
 * Copyright (C) 2025 Ultralight, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSObjectRef_h
#define JSObjectRef_h

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/WebKitAvailability.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h> /* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*!
@enum JSPropertyAttribute
@constant kJSPropertyAttributeNone         Specifies that a property has no special attributes.
@constant kJSPropertyAttributeReadOnly     Specifies that a property is read-only.
@constant kJSPropertyAttributeDontEnum     Specifies that a property should not be enumerated by JSPropertyEnumerators and JavaScript for...in loops.
@constant kJSPropertyAttributeDontDelete   Specifies that the delete operation should fail on a property.
*/
enum { 
    kJSPropertyAttributeNone         = 0,
    kJSPropertyAttributeReadOnly     = 1 << 1,
    kJSPropertyAttributeDontEnum     = 1 << 2,
    kJSPropertyAttributeDontDelete   = 1 << 3
};

/*! 
@typedef JSPropertyAttributes
@abstract A set of JSPropertyAttributes. Combine multiple attributes by logically ORing them together.
*/
typedef unsigned JSPropertyAttributes;

/*!
@enum JSClassAttribute
@constant kJSClassAttributeNone Specifies that a class has no special attributes.
@constant kJSClassAttributeNoAutomaticPrototype Specifies that a class should not automatically generate a shared prototype for its instance objects. Use kJSClassAttributeNoAutomaticPrototype in combination with JSObjectSetPrototype to manage prototypes manually.
*/
enum { 
    kJSClassAttributeNone = 0,
    kJSClassAttributeNoAutomaticPrototype = 1 << 1
};

/*! 
@typedef JSClassAttributes
@abstract A set of JSClassAttributes. Combine multiple attributes by logically ORing them together.
*/
typedef unsigned JSClassAttributes;

/*! 
@typedef JSObjectInitializeCallback
@abstract The callback invoked when an object is first created.
@param ctx The execution context to use.
@param object The JSObject being created.
@discussion If you named your function Initialize, you would declare it like this:

void Initialize(JSContextRef ctx, JSObjectRef object);

Unlike the other object callbacks, the initialize callback is called on the least
derived class (the parent class) first, and the most derived class last.
*/
typedef void
(*JSObjectInitializeCallback) (JSContextRef ctx, JSObjectRef object);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef void
(*JSObjectInitializeCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef object);

/*!
@typedef JSObjectFinalizeCallback
@abstract The callback invoked when an object is finalized (prepared for garbage collection). An object may be finalized on any thread.
@param object The JSObject being finalized.
@discussion If you named your function Finalize, you would declare it like this:

void Finalize(JSObjectRef object);

The finalize callback is called on the most derived class first, and the least 
derived class (the parent class) last.

You must not call any function that may cause a garbage collection or an allocation
of a garbage collected object from within a JSObjectFinalizeCallback. This includes
all functions that have a JSContextRef parameter.
*/
typedef void
(*JSObjectFinalizeCallback) (JSObjectRef object);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef void
(*JSObjectFinalizeCallbackEx) (JSClassRef jsClass, JSObjectRef object);

/*! 
@typedef JSObjectHasPropertyCallback
@abstract The callback invoked when determining whether an object has a property.
@param ctx The execution context to use.
@param object The JSObject to search for the property.
@param propertyName A JSString containing the name of the property look up.
@result true if object has the property, otherwise false.
@discussion If you named your function HasProperty, you would declare it like this:

bool HasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName);

If this function returns false, the hasProperty request forwards to object's statically declared properties, then its parent class chain (which includes the default object class), then its prototype chain.

This callback enables optimization in cases where only a property's existence needs to be known, not its value, and computing its value would be expensive.

If this callback is NULL, the getProperty callback will be used to service hasProperty requests.
*/
typedef bool
(*JSObjectHasPropertyCallback) (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef bool
(*JSObjectHasPropertyCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef object, JSStringRef propertyName);

/*! 
@typedef JSObjectGetPropertyCallback
@abstract The callback invoked when getting a property's value.
@param ctx The execution context to use.
@param object The JSObject to search for the property.
@param propertyName A JSString containing the name of the property to get.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result The property's value if object has the property, otherwise NULL.
@discussion If you named your function GetProperty, you would declare it like this:

JSValueRef GetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

If this function returns NULL, the get request forwards to object's statically declared properties, then its parent class chain (which includes the default object class), then its prototype chain.
*/
typedef JSValueRef
(*JSObjectGetPropertyCallback) (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef JSValueRef
(*JSObjectGetPropertyCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

/*! 
@typedef JSObjectSetPropertyCallback
@abstract The callback invoked when setting a property's value.
@param ctx The execution context to use.
@param object The JSObject on which to set the property's value.
@param propertyName A JSString containing the name of the property to set.
@param value A JSValue to use as the property's value.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result true if the property was set, otherwise false.
@discussion If you named your function SetProperty, you would declare it like this:

bool SetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception);

If this function returns false, the set request forwards to object's statically declared properties, then its parent class chain (which includes the default object class).
*/
typedef bool
(*JSObjectSetPropertyCallback) (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef bool
(*JSObjectSetPropertyCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception);

/*! 
@typedef JSObjectDeletePropertyCallback
@abstract The callback invoked when deleting a property.
@param ctx The execution context to use.
@param object The JSObject in which to delete the property.
@param propertyName A JSString containing the name of the property to delete.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result true if propertyName was successfully deleted, otherwise false.
@discussion If you named your function DeleteProperty, you would declare it like this:

bool DeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

If this function returns false, the delete request forwards to object's statically declared properties, then its parent class chain (which includes the default object class).
*/
typedef bool
(*JSObjectDeletePropertyCallback) (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef bool
(*JSObjectDeletePropertyCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

/*! 
@typedef JSObjectGetPropertyNamesCallback
@abstract The callback invoked when collecting the names of an object's properties.
@param ctx The execution context to use.
@param object The JSObject whose property names are being collected.
@param propertyNames A JavaScript property name accumulator in which to accumulate the names of object's properties.
@discussion If you named your function GetPropertyNames, you would declare it like this:

void GetPropertyNames(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames);

Property name accumulators are used by JSObjectCopyPropertyNames and JavaScript for...in loops. 

Use JSPropertyNameAccumulatorAddName to add property names to accumulator. A class's getPropertyNames callback only needs to provide the names of properties that the class vends through a custom getProperty or setProperty callback. Other properties, including statically declared properties, properties vended by other classes, and properties belonging to object's prototype, are added independently.
*/
typedef void
(*JSObjectGetPropertyNamesCallback) (JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef void
(*JSObjectGetPropertyNamesCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames);

/*! 
@typedef JSObjectCallAsFunctionCallback
@abstract The callback invoked when an object is called as a function.
@param ctx The execution context to use.
@param function A JSObject that is the function being called.
@param thisObject A JSObject that is the 'this' variable in the function's scope.
@param argumentCount An integer count of the number of arguments in arguments.
@param arguments A JSValue array of the  arguments passed to the function.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result A JSValue that is the function's return value.
@discussion If you named your function CallAsFunction, you would declare it like this:

JSValueRef CallAsFunction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

If your callback were invoked by the JavaScript expression 'myObject.myFunction()', function would be set to myFunction, and thisObject would be set to myObject.

If this callback is NULL, calling your object as a function will throw an exception.
*/
typedef JSValueRef
(*JSObjectCallAsFunctionCallback) (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef JSValueRef
(*JSObjectCallAsFunctionCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSStringRef className, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/*!
@typedef JSObjectCallAsFunctionDataCallback
@abstract The callback invoked when a function created with JSObjectMakeFunctionWithData is called.
@param ctx The execution context to use.
@param userData The user data pointer the function was created with.
@param function A JSObject that is the function being called.
@param thisObject A JSObject that is the 'this' variable in the function's scope.
@param argumentCount An integer count of the number of arguments in arguments.
@param arguments A JSValue array of the arguments passed to the function.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result A JSValue that is the function's return value.
*/
typedef JSValueRef
(*JSObjectCallAsFunctionDataCallback) (JSContextRef ctx, void* userData, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/*!
@typedef JSFunctionDataFinalizeCallback
@abstract The callback invoked when a function created with JSObjectMakeFunctionWithData is garbage collected.
@param userData The user data pointer the function was created with.
@discussion Called exactly once, either when the function object is garbage collected or when its
 context group is destroyed. It runs with the context group's internal lock held: you must not call
 any function that may cause a garbage collection or an allocation of a garbage collected object
 from within a JSFunctionDataFinalizeCallback. This includes all functions that have a JSContextRef
 parameter.
*/
typedef void
(*JSFunctionDataFinalizeCallback) (void* userData);

/*! 
@typedef JSObjectCallAsConstructorCallback
@abstract The callback invoked when an object is used as a constructor in a 'new' expression.
@param ctx The execution context to use.
@param constructor A JSObject that is the constructor being called.
@param argumentCount An integer count of the number of arguments in arguments.
@param arguments A JSValue array of the  arguments passed to the function.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result A JSObject that is the constructor's return value.
@discussion If you named your function CallAsConstructor, you would declare it like this:

JSObjectRef CallAsConstructor(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

If your callback were invoked by the JavaScript expression 'new myConstructor()', constructor would be set to myConstructor.

If this callback is NULL, using your object as a constructor in a 'new' expression will throw an exception.
*/
typedef JSObjectRef
(*JSObjectCallAsConstructorCallback) (JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef JSObjectRef
(*JSObjectCallAsConstructorCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/*! 
@typedef JSObjectHasInstanceCallback
@abstract hasInstance The callback invoked when an object is used as the target of an 'instanceof' expression.
@param ctx The execution context to use.
@param constructor The JSObject that is the target of the 'instanceof' expression.
@param possibleInstance The JSValue being tested to determine if it is an instance of constructor.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result true if possibleInstance is an instance of constructor, otherwise false.
@discussion If you named your function HasInstance, you would declare it like this:

bool HasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);

If your callback were invoked by the JavaScript expression 'someValue instanceof myObject', constructor would be set to myObject and possibleInstance would be set to someValue.

If this callback is NULL, 'instanceof' expressions that target your object will return false.

Standard JavaScript practice calls for objects that implement the callAsConstructor callback to implement the hasInstance callback as well.
*/
typedef bool
(*JSObjectHasInstanceCallback)  (JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef bool
(*JSObjectHasInstanceCallbackEx)  (JSContextRef ctx, JSClassRef jsClass, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);

/*! 
@typedef JSObjectConvertToTypeCallback
@abstract The callback invoked when converting an object to a particular JavaScript type.
@param ctx The execution context to use.
@param object The JSObject to convert.
@param type A JSType specifying the JavaScript type to convert to.
@param exception A pointer to a JSValueRef in which to return an exception, if any.
@result The objects's converted value, or NULL if the object was not converted.
@discussion If you named your function ConvertToType, you would declare it like this:

JSValueRef ConvertToType(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception);

If this function returns false, the conversion request forwards to object's parent class chain (which includes the default object class).

This function is only invoked when converting an object to number or string. An object converted to boolean is 'true.' An object converted to object is itself.
*/
typedef JSValueRef
(*JSObjectConvertToTypeCallback) (JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception);

/* Extension of the above callback with the class that the method is being invoked for. */
typedef JSValueRef
(*JSObjectConvertToTypeCallbackEx) (JSContextRef ctx, JSClassRef jsClass, JSObjectRef object, JSType type, JSValueRef* exception);

/*! 
@struct JSStaticValue
@abstract This structure describes a statically declared value property.
@field name A null-terminated UTF8 string containing the property's name.
@field getProperty A JSObjectGetPropertyCallback to invoke when getting the property's value.
@field setProperty A JSObjectSetPropertyCallback to invoke when setting the property's value. May be NULL if the ReadOnly attribute is set.
@field attributes A logically ORed set of JSPropertyAttributes to give to the property.
*/
typedef struct {
    const char* name;
    JSObjectGetPropertyCallback getProperty;
    JSObjectSetPropertyCallback setProperty;
    JSPropertyAttributes attributes;
} JSStaticValue;

/* Extended static value structure for version 1000 classes */
typedef struct {
    const char* name;
    JSObjectGetPropertyCallbackEx getPropertyEx;
    JSObjectSetPropertyCallbackEx setPropertyEx;
    JSPropertyAttributes attributes;
} JSStaticValueEx;

/*! 
@struct JSStaticFunction
@abstract This structure describes a statically declared function property.
@field name A null-terminated UTF8 string containing the property's name.
@field callAsFunction A JSObjectCallAsFunctionCallback to invoke when the property is called as a function.
@field attributes A logically ORed set of JSPropertyAttributes to give to the property.
*/
typedef struct {
    const char* name;
    JSObjectCallAsFunctionCallback callAsFunction;
    JSPropertyAttributes attributes;
} JSStaticFunction;

/* Extended static function structure for version 1000 classes */
typedef struct {
    const char* name;
    JSObjectCallAsFunctionCallbackEx callAsFunctionEx;
    JSPropertyAttributes attributes;
} JSStaticFunctionEx;

/*!
@struct JSClassDefinition
@abstract This structure contains properties and callbacks that define a type of object. All fields other than the version field are optional. Any pointer may be NULL.
@field version The version number of this structure. Use 0 for standard callbacks or 1000 for extended callbacks.
@field attributes A logically ORed set of JSClassAttributes to give to the class.
@field className A null-terminated UTF8 string containing the class's name.
@field parentClass A JSClass to set as the class's parent class. Pass NULL use the default object class.
@field staticValues (Version 0) A JSStaticValue array containing the class's statically declared value properties. Pass NULL to specify no statically declared value properties. The array must be terminated by a JSStaticValue whose name field is NULL.
@field staticFunctions (Version 0) A JSStaticFunction array containing the class's statically declared function properties. Pass NULL to specify no statically declared function properties. The array must be terminated by a JSStaticFunction whose name field is NULL.
@field staticValuesEx (Version 1000) A JSStaticValueEx array for extended static values that receive JSClassRef in callbacks.
@field staticFunctionsEx (Version 1000) A JSStaticFunctionEx array for extended static functions that receive JSClassRef in callbacks.
@field initialize/initializeEx The callback invoked when an object is first created. Use version 0 'initialize' or version 1000 'initializeEx'.
@field finalize/finalizeEx The callback invoked when an object is finalized (prepared for garbage collection). Use version 0 'finalize' or version 1000 'finalizeEx'.
@field hasProperty/hasPropertyEx The callback invoked when determining whether an object has a property. If this field is NULL, getProperty is called instead. Use version 0 'hasProperty' or version 1000 'hasPropertyEx'.
@field getProperty/getPropertyEx The callback invoked when getting a property's value. Use version 0 'getProperty' or version 1000 'getPropertyEx'.
@field setProperty/setPropertyEx The callback invoked when setting a property's value. Use version 0 'setProperty' or version 1000 'setPropertyEx'.
@field deleteProperty/deletePropertyEx The callback invoked when deleting a property. Use version 0 'deleteProperty' or version 1000 'deletePropertyEx'.
@field getPropertyNames/getPropertyNamesEx The callback invoked when collecting the names of an object's properties. Use version 0 'getPropertyNames' or version 1000 'getPropertyNamesEx'.
@field callAsFunction/callAsFunctionEx The callback invoked when an object is called as a function. Use version 0 'callAsFunction' or version 1000 'callAsFunctionEx'.
@field hasInstance/hasInstanceEx The callback invoked when an object is used as the target of an 'instanceof' expression. Use version 0 'hasInstance' or version 1000 'hasInstanceEx'.
@field callAsConstructor/callAsConstructorEx The callback invoked when an object is used as a constructor in a 'new' expression. Use version 0 'callAsConstructor' or version 1000 'callAsConstructorEx'.
@field convertToType/convertToTypeEx The callback invoked when converting an object to a particular JavaScript type. Use version 0 'convertToType' or version 1000 'convertToTypeEx'.
@field privateData Private data storage for the JSClass itself (version 1000 only). This allows you to associate arbitrary data with the class definition.

@discussion Version System and Callback Types:

The JSClassDefinition structure supports two versions of callbacks through a union with anonymous structs:

Version 0 (Standard Callbacks):
- Original JSC API callbacks that receive standard parameters
- Callbacks do not receive the JSClassRef, limiting context awareness
- Use this version for simple cases or backward compatibility
- Access callbacks directly: definition.initialize, definition.getProperty, etc.
- Example callback: void Initialize(JSContextRef ctx, JSObjectRef object)

Version 1000 (Extended Callbacks):
- Enhanced callbacks that receive JSClassRef as an additional first parameter after JSContextRef
- Enables callbacks to know which class they belong to, useful for:
  * Implementing class hierarchies with shared callback implementations
  * Storing per-class data via the privateData field
  * Creating more object-oriented bindings
- Access callbacks with 'Ex' suffix: definition.initializeEx, definition.getPropertyEx, etc.
- Example callback: void InitializeEx(JSContextRef ctx, JSClassRef jsClass, JSObjectRef object)
- The privateData field is ONLY available for version 1000 classes

The union structure allows both callback sets to share the same memory space, with anonymous structs
enabling direct field access based on version (e.g., definition.initialize for v0 or definition.initializeEx for v1000).

Example Usage:

Version 0 (Standard):
JSClassDefinition definition = kJSClassDefinitionEmpty;
definition.version = 0;  // or omit since 0 is default
definition.className = "MyClass";
definition.initialize = MyInitialize;  // Direct access
definition.finalize = MyFinalize;

Version 1000 (Extended):
JSClassDefinition definition = kJSClassDefinitionEmpty;
definition.version = 1000;
definition.className = "MyClass";
definition.initializeEx = MyInitializeEx;  // Direct access with Ex suffix
definition.finalizeEx = MyFinalizeEx;
definition.privateData = myClassData;  // Store class-specific data

JSClassRef jsClass = JSClassCreate(&definition);
// Later retrieve private data in callbacks:
void* data = JSClassGetPrivate(jsClass);

@discussion The staticValues and staticFunctions arrays are the simplest and most efficient means for vending custom properties. Statically declared properties autmatically service requests like getProperty, setProperty, and getPropertyNames. Property access callbacks are required only to implement unusual properties, like array indexes, whose names are not known at compile-time.

If you named your getter function "GetX" and your setter function "SetX", you would declare a JSStaticValue array containing "X" like this:

JSStaticValue StaticValueArray[] = {
    { "X", GetX, SetX, kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

Standard JavaScript practice calls for storing function objects in prototypes, so they can be shared. The default JSClass created by JSClassCreate follows this idiom, instantiating objects with a shared, automatically generating prototype containing the class's function objects. The kJSClassAttributeNoAutomaticPrototype attribute specifies that a JSClass should not automatically generate such a prototype. The resulting JSClass instantiates objects with the default object prototype, and gives each instance object its own copy of the class's function objects.

A NULL callback specifies that the default object callback should substitute, except in the case of hasProperty, where it specifies that getProperty should substitute.

It is not possible to use JS subclassing with objects created from a class definition that sets callAsConstructor by default. Subclassing is supported via the JSObjectMakeConstructor function, however.
*/
typedef struct {
    int                                 version; /* version is 0 for standard callbacks or 1000 for extended callbacks */
    JSClassAttributes                   attributes;

    const char*                         className;
    JSClassRef                          parentClass;

    union {
        struct {
            const JSStaticValue*                staticValues;
            const JSStaticFunction*             staticFunctions;

            JSObjectInitializeCallback          initialize;
            JSObjectFinalizeCallback            finalize;
            JSObjectHasPropertyCallback         hasProperty;
            JSObjectGetPropertyCallback         getProperty;
            JSObjectSetPropertyCallback         setProperty;
            JSObjectDeletePropertyCallback      deleteProperty;
            JSObjectGetPropertyNamesCallback    getPropertyNames;
            JSObjectCallAsFunctionCallback      callAsFunction;
            JSObjectCallAsConstructorCallback   callAsConstructor;
            JSObjectHasInstanceCallback         hasInstance;
            JSObjectConvertToTypeCallback       convertToType;
        }; /* anonymous struct for version 0 - maintains backward compatibility */

        struct {
            const JSStaticValueEx*              staticValuesEx;
            const JSStaticFunctionEx*           staticFunctionsEx;

            JSObjectInitializeCallbackEx        initializeEx;
            JSObjectFinalizeCallbackEx          finalizeEx;
            JSObjectHasPropertyCallbackEx       hasPropertyEx;
            JSObjectGetPropertyCallbackEx       getPropertyEx;
            JSObjectSetPropertyCallbackEx       setPropertyEx;
            JSObjectDeletePropertyCallbackEx    deletePropertyEx;
            JSObjectGetPropertyNamesCallbackEx  getPropertyNamesEx;
            JSObjectCallAsFunctionCallbackEx    callAsFunctionEx;
            JSObjectCallAsConstructorCallbackEx callAsConstructorEx;
            JSObjectHasInstanceCallbackEx       hasInstanceEx;
            JSObjectConvertToTypeCallbackEx     convertToTypeEx;
        }; /* anonymous struct for version 1000 - maintains clean API */
    };

    void* privateData; /* Private data for version 1000 classes */
} JSClassDefinition;

/*! 
@const kJSClassDefinitionEmpty 
@abstract A JSClassDefinition structure of the current version, filled with NULL pointers and having no attributes.
@discussion Use this constant as a convenience when creating class definitions. For example, to create a class definition with only a finalize method:

JSClassDefinition definition = kJSClassDefinitionEmpty;
definition.finalize = Finalize;
*/
JS_EXPORT extern const JSClassDefinition kJSClassDefinitionEmpty;

/*!
@function
@abstract Creates a JavaScript class suitable for use with JSObjectMake.
@param definition A JSClassDefinition that defines the class.
@result A JSClass with the given definition. Ownership follows the Create Rule.
*/
JS_EXPORT JSClassRef JSClassCreate(const JSClassDefinition* definition);

/*!
@function
@abstract Retains a JavaScript class.
@param jsClass The JSClass to retain.
@result A JSClass that is the same as jsClass.
*/
JS_EXPORT JSClassRef JSClassRetain(JSClassRef jsClass);

/*!
@function
@abstract Releases a JavaScript class.
@param jsClass The JSClass to release.
*/
JS_EXPORT void JSClassRelease(JSClassRef jsClass);

/*!
@function
@abstract Creates a JavaScript object.
@param ctx The execution context to use.
@param jsClass The JSClass to assign to the object. Pass NULL to use the default object class.
@param data A void* to set as the object's private data. Pass NULL to specify no private data.
@result A JSObject with the given class and private data.
@discussion The default object class does not allocate storage for private data, so you must provide a non-NULL jsClass to JSObjectMake if you want your object to be able to store private data.

data is set on the created object before the intialize methods in its class chain are called. This enables the initialize methods to retrieve and manipulate data through JSObjectGetPrivate.
*/
JS_EXPORT JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data);

/*!
@function
@abstract Convenience method for creating a JavaScript function with a given callback as its implementation.
@param ctx The execution context to use.
@param name A JSString containing the function's name. This will be used when converting the function to string. Pass NULL to create an anonymous function.
@param callAsFunction The JSObjectCallAsFunctionCallback to invoke when the function is called.
@result A JSObject that is a function. The object's prototype will be the default function prototype.
*/
JS_EXPORT JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionCallback callAsFunction);

/*!
@function
@abstract Convenience method for creating a JavaScript constructor.
@param ctx The execution context to use.
@param jsClass A JSClass that is the class your constructor will assign to the objects its constructs. jsClass will be used to set the constructor's .prototype property, and to evaluate 'instanceof' expressions. Pass NULL to use the default object class.
@param callAsConstructor A JSObjectCallAsConstructorCallback to invoke when your constructor is used in a 'new' expression. Pass NULL to use the default object constructor.
@result A JSObject that is a constructor. The object's prototype will be the default object prototype.
@discussion The default object constructor takes no arguments and constructs an object of class jsClass with no private data. If the constructor is inherited via JS subclassing and the value returned from callAsConstructor was created with jsClass, then the returned object will have it's prototype overridden to the derived class's prototype.
*/
JS_EXPORT JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor);

/*!
 @function
 @abstract Creates a JavaScript Array object.
 @param ctx The execution context to use.
 @param argumentCount An integer count of the number of arguments in arguments.
 @param arguments A JSValue array of data to populate the Array with. Pass NULL if argumentCount is 0.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result A JSObject that is an Array.
 @discussion The behavior of this function does not exactly match the behavior of the built-in Array constructor. Specifically, if one argument 
 is supplied, this function returns an array with one element.
 */
JS_EXPORT JSObjectRef JSObjectMakeArray(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) JSC_API_AVAILABLE(macos(10.6), ios(7.0));

/*!
 @function
 @abstract Creates a JavaScript Date object, as if by invoking the built-in Date constructor.
 @param ctx The execution context to use.
 @param argumentCount An integer count of the number of arguments in arguments.
 @param arguments A JSValue array of arguments to pass to the Date Constructor. Pass NULL if argumentCount is 0.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result A JSObject that is a Date.
 */
JS_EXPORT JSObjectRef JSObjectMakeDate(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) JSC_API_AVAILABLE(macos(10.6), ios(7.0));

/*!
 @function
 @abstract Creates a JavaScript Error object, as if by invoking the built-in Error constructor.
 @param ctx The execution context to use.
 @param argumentCount An integer count of the number of arguments in arguments.
 @param arguments A JSValue array of arguments to pass to the Error Constructor. Pass NULL if argumentCount is 0.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result A JSObject that is an Error.
 */
JS_EXPORT JSObjectRef JSObjectMakeError(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) JSC_API_AVAILABLE(macos(10.6), ios(7.0));

/*!
 @function
 @abstract Creates a JavaScript RegExp object, as if by invoking the built-in RegExp constructor.
 @param ctx The execution context to use.
 @param argumentCount An integer count of the number of arguments in arguments.
 @param arguments A JSValue array of arguments to pass to the RegExp Constructor. Pass NULL if argumentCount is 0.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result A JSObject that is a RegExp.
 */
JS_EXPORT JSObjectRef JSObjectMakeRegExp(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) JSC_API_AVAILABLE(macos(10.6), ios(7.0));

/*!
 @function
 @abstract Creates a JavaScript promise object by invoking the provided executor.
 @param ctx The execution context to use.
 @param resolve A pointer to a JSObjectRef in which to store the resolve function for the new promise. Pass NULL if you do not care to store the resolve callback.
 @param reject A pointer to a JSObjectRef in which to store the reject function for the new promise. Pass NULL if you do not care to store the reject callback.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result A JSObject that is a promise or NULL if an exception occurred.
 */
JS_EXPORT JSObjectRef JSObjectMakeDeferredPromise(JSContextRef ctx, JSObjectRef* resolve, JSObjectRef* reject, JSValueRef* exception) JSC_API_AVAILABLE(macos(10.15), ios(13.0));

/*!
@function
@abstract Creates a function with a given script as its body.
@param ctx The execution context to use.
@param name A JSString containing the function's name. This will be used when converting the function to string. Pass NULL to create an anonymous function.
@param parameterCount An integer count of the number of parameter names in parameterNames.
@param parameterNames A JSString array containing the names of the function's parameters. Pass NULL if parameterCount is 0.
@param body A JSString containing the script to use as the function's body.
@param sourceURL A JSString containing a URL for the script's source file. This is only used when reporting exceptions. Pass NULL if you do not care to include source file information in exceptions.
@param startingLineNumber An integer value specifying the script's starting line number in the file located at sourceURL. This is only used when reporting exceptions. The value is one-based, so the first line is line 1 and invalid values are clamped to 1.
@param exception A pointer to a JSValueRef in which to store a syntax error exception, if any. Pass NULL if you do not care to store a syntax error exception.
@result A JSObject that is a function, or NULL if either body or parameterNames contains a syntax error. The object's prototype will be the default function prototype.
@discussion Use this method when you want to execute a script repeatedly, to avoid the cost of re-parsing the script before each execution.
*/
JS_EXPORT JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception);

/*!
@function
@abstract Gets an object's prototype.
@param ctx  The execution context to use.
@param object A JSObject whose prototype you want to get.
@result A JSValue that is the object's prototype.
*/
JS_EXPORT JSValueRef JSObjectGetPrototype(JSContextRef ctx, JSObjectRef object);

/*!
@function
@abstract Sets an object's prototype.
@param ctx  The execution context to use.
@param object The JSObject whose prototype you want to set.
@param value A JSValue to set as the object's prototype.
*/
JS_EXPORT void JSObjectSetPrototype(JSContextRef ctx, JSObjectRef object, JSValueRef value);

/*!
@function
@abstract Tests whether an object has a given property.
@param object The JSObject to test.
@param propertyName A JSString containing the property's name.
@result true if the object has a property whose name matches propertyName, otherwise false.
*/
JS_EXPORT bool JSObjectHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName);

/*!
@function
@abstract Gets a property from an object.
@param ctx The execution context to use.
@param object The JSObject whose property you want to get.
@param propertyName A JSString containing the property's name.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result The property's value if object has the property, otherwise the undefined value.
*/
JS_EXPORT JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

/*!
@function
@abstract Sets a property on an object.
@param ctx The execution context to use.
@param object The JSObject whose property you want to set.
@param propertyName A JSString containing the property's name.
@param value A JSValueRef to use as the property's value.
@param attributes A logically ORed set of JSPropertyAttributes to give to the property.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
*/
JS_EXPORT void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception);

/*!
@function
@abstract Deletes a property from an object.
@param ctx The execution context to use.
@param object The JSObject whose property you want to delete.
@param propertyName A JSString containing the property's name.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result true if the delete operation succeeds, otherwise false (for example, if the property has the kJSPropertyAttributeDontDelete attribute set).
*/
JS_EXPORT bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

/*!
 @function
 @abstract Tests whether an object has a given property using a JSValueRef as the property key.
 @param object The JSObject to test.
 @param propertyKey A JSValueRef containing the property key to use when looking up the property.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result true if the object has a property whose name matches propertyKey, otherwise false.
 @discussion This function is the same as performing "propertyKey in object" from JavaScript.
 */
JS_EXPORT bool JSObjectHasPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef propertyKey, JSValueRef* exception) JSC_API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @function
 @abstract Gets a property from an object using a JSValueRef as the property key.
 @param ctx The execution context to use.
 @param object The JSObject whose property you want to get.
 @param propertyKey A JSValueRef containing the property key to use when looking up the property.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result The property's value if object has the property key, otherwise the undefined value.
 @discussion This function is the same as performing "object[propertyKey]" from JavaScript.
 */
JS_EXPORT JSValueRef JSObjectGetPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef propertyKey, JSValueRef* exception) JSC_API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @function
 @abstract Sets a property on an object using a JSValueRef as the property key.
 @param ctx The execution context to use.
 @param object The JSObject whose property you want to set.
 @param propertyKey A JSValueRef containing the property key to use when looking up the property.
 @param value A JSValueRef to use as the property's value.
 @param attributes A logically ORed set of JSPropertyAttributes to give to the property.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @discussion This function is the same as performing "object[propertyKey] = value" from JavaScript.
 */
JS_EXPORT void JSObjectSetPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef propertyKey, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception) JSC_API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @function
 @abstract Deletes a property from an object using a JSValueRef as the property key.
 @param ctx The execution context to use.
 @param object The JSObject whose property you want to delete.
 @param propertyKey A JSValueRef containing the property key to use when looking up the property.
 @param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
 @result true if the delete operation succeeds, otherwise false (for example, if the property has the kJSPropertyAttributeDontDelete attribute set).
 @discussion This function is the same as performing "delete object[propertyKey]" from JavaScript.
 */
JS_EXPORT bool JSObjectDeletePropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef propertyKey, JSValueRef* exception) JSC_API_AVAILABLE(macos(10.15), ios(13.0));

/*!
@function
@abstract Gets a property from an object by numeric index.
@param ctx The execution context to use.
@param object The JSObject whose property you want to get.
@param propertyIndex An integer value that is the property's name.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result The property's value if object has the property, otherwise the undefined value.
@discussion Calling JSObjectGetPropertyAtIndex is equivalent to calling JSObjectGetProperty with a string containing propertyIndex, but JSObjectGetPropertyAtIndex provides optimized access to numeric properties.
*/
JS_EXPORT JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception);

/*!
@function
@abstract Sets a property on an object by numeric index.
@param ctx The execution context to use.
@param object The JSObject whose property you want to set.
@param propertyIndex The property's name as a number.
@param value A JSValue to use as the property's value.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@discussion Calling JSObjectSetPropertyAtIndex is equivalent to calling JSObjectSetProperty with a string containing propertyIndex, but JSObjectSetPropertyAtIndex provides optimized access to numeric properties.
*/
JS_EXPORT void JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef value, JSValueRef* exception);

/*!
@function
@abstract Gets an object's private data.
@param object A JSObject whose private data you want to get.
@result A void* that is the object's private data, if the object has private data, otherwise NULL.
*/
JS_EXPORT void* JSObjectGetPrivate(JSObjectRef object);

/*!
@function
@abstract Sets a pointer to private data on an object.
@param object The JSObject whose private data you want to set.
@param data A void* to set as the object's private data.
@result true if object can store private data, otherwise false.
@discussion The default object class does not allocate storage for private data. Only objects created with a non-NULL JSClass can store private data.
*/
JS_EXPORT bool JSObjectSetPrivate(JSObjectRef object, void* data);

/*!
@function
@abstract Tests whether an object can be called as a function.
@param ctx  The execution context to use.
@param object The JSObject to test.
@result true if the object can be called as a function, otherwise false.
*/
JS_EXPORT bool JSObjectIsFunction(JSContextRef ctx, JSObjectRef object);

/*!
@function
@abstract Calls an object as a function.
@param ctx The execution context to use.
@param object The JSObject to call as a function.
@param thisObject The object to use as "this," or NULL to use the global object as "this."
@param argumentCount An integer count of the number of arguments in arguments.
@param arguments A JSValue array of arguments to pass to the function. Pass NULL if argumentCount is 0.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result The JSValue that results from calling object as a function, or NULL if an exception is thrown or object is not a function.
*/
JS_EXPORT JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/*!
@function
@abstract Tests whether an object can be called as a constructor.
@param ctx  The execution context to use.
@param object The JSObject to test.
@result true if the object can be called as a constructor, otherwise false.
*/
JS_EXPORT bool JSObjectIsConstructor(JSContextRef ctx, JSObjectRef object);

/*!
@function
@abstract Calls an object as a constructor.
@param ctx The execution context to use.
@param object The JSObject to call as a constructor.
@param argumentCount An integer count of the number of arguments in arguments.
@param arguments A JSValue array of arguments to pass to the constructor. Pass NULL if argumentCount is 0.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result The JSObject that results from calling object as a constructor, or NULL if an exception is thrown or object is not a constructor.
*/
JS_EXPORT JSObjectRef JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef object, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/*!
@function
@abstract Gets the names of an object's enumerable properties.
@param ctx The execution context to use.
@param object The object whose property names you want to get.
@result A JSPropertyNameArray containing the names object's enumerable properties. Ownership follows the Create Rule.
*/
JS_EXPORT JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef object);

/*!
@function
@abstract Retains a JavaScript property name array.
@param array The JSPropertyNameArray to retain.
@result A JSPropertyNameArray that is the same as array.
*/
JS_EXPORT JSPropertyNameArrayRef JSPropertyNameArrayRetain(JSPropertyNameArrayRef array);

/*!
@function
@abstract Releases a JavaScript property name array.
@param array The JSPropetyNameArray to release.
*/
JS_EXPORT void JSPropertyNameArrayRelease(JSPropertyNameArrayRef array);

/*!
@function
@abstract Gets a count of the number of items in a JavaScript property name array.
@param array The array from which to retrieve the count.
@result An integer count of the number of names in array.
*/
JS_EXPORT size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef array);

/*!
@function
@abstract Gets a property name at a given index in a JavaScript property name array.
@param array The array from which to retrieve the property name.
@param index The index of the property name to retrieve.
@result A JSStringRef containing the property name.
*/
JS_EXPORT JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef array, size_t index);

/*!
@function
@abstract Adds a property name to a JavaScript property name accumulator.
@param accumulator The accumulator object to which to add the property name.
@param propertyName The property name to add.
*/
JS_EXPORT void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef accumulator, JSStringRef propertyName);

/*!
@function
@abstract Gets the private data from a JSClass (version 1000 only).
@param jsClass The JSClass whose private data you want to get.
@result The private data for the class, or NULL if the class is not version 1000.
*/
JS_EXPORT void* JSClassGetPrivate(JSClassRef jsClass);

/*!
@function
@abstract Sets the private data for a JSClass (version 1000 only).
@param jsClass The JSClass whose private data you want to set.
@param data The private data to set.
@result true if the private data was set (class is version 1000), false otherwise.
*/
JS_EXPORT bool JSClassSetPrivate(JSClassRef jsClass, void* data);

/*!
@function
@abstract Creates a JavaScript function with a callback, a user data pointer, and an optional finalizer.
@param ctx The execution context to use.
@param name A JSString containing the function's name. This will be used when converting the function to string. Pass NULL to create an anonymous function.
@param callAsFunction The JSObjectCallAsFunctionDataCallback to invoke when the function is called. The userData pointer is passed to every invocation.
@param userData A pointer to user data associated with the function. May be NULL.
@param finalize A JSFunctionDataFinalizeCallback invoked exactly once when the function is garbage collected or its context group is destroyed. Pass NULL if no cleanup is needed.
@result A JSObject that is a function. The object's prototype will be the default function prototype.
@discussion Unlike objects faked as callable through a JSClass, the returned object is a real
 function: it inherits from Function.prototype, so bind, call, and apply work on it from script.
*/
JS_EXPORT JSObjectRef JSObjectMakeFunctionWithData(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionDataCallback callAsFunction, void* userData, JSFunctionDataFinalizeCallback finalize);

/*!
@function
@abstract Gets multiple properties of an object in one call, using UTF-8 property names.
@param ctx The execution context to use.
@param object The JSObject whose properties you want to get.
@param names An array of count null-terminated UTF-8 property names.
@param count The number of properties to get.
@param outValues An array of count JSValueRefs that receives the property values. A property that does not exist yields the undefined value.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result true if all properties were read; false if an exception was thrown, in which case the contents of outValues are unspecified.
*/
JS_EXPORT bool JSObjectGetPropertiesUTF8(JSContextRef ctx, JSObjectRef object, const char* const* names, size_t count, JSValueRef* outValues, JSValueRef* exception);

/*!
@function
@abstract Sets multiple properties of an object in one call, using UTF-8 property names.
@param ctx The execution context to use.
@param object The JSObject whose properties you want to set.
@param names An array of count null-terminated UTF-8 property names.
@param values An array of count JSValueRefs that are the property values to set.
@param count The number of properties to set.
@param attributes A logically ORed set of JSPropertyAttributes to give to each property.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result true if all properties were set; false if an exception was thrown. Properties before the failing one remain set.
*/
JS_EXPORT bool JSObjectSetPropertiesUTF8(JSContextRef ctx, JSObjectRef object, const char* const* names, const JSValueRef* values, size_t count, unsigned attributes, JSValueRef* exception);

/*!
@function
@abstract Creates a JavaScript object initialized with a set of properties, using UTF-8 property names.
@param ctx The execution context to use.
@param names An array of count null-terminated UTF-8 property names.
@param values An array of count JSValueRefs that are the property values.
@param count The number of properties.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result A JSObject with the given properties, or NULL if an exception was thrown.
*/
JS_EXPORT JSObjectRef JSObjectMakeWithPropertiesUTF8(JSContextRef ctx, const char* const* names, const JSValueRef* values, size_t count, JSValueRef* exception);

/*!
@function
@abstract Gets an object's own enumerable string-keyed properties (names and values) in one call.
@param ctx The execution context to use.
@param object The JSObject whose entries you want to get.
@param start The enumeration-order position to start reading entries from (0 reads from the beginning). Positions at or beyond the property count read nothing.
@param outNames An array that receives up to capacity JSStringRefs, one per property name. May be NULL if capacity is 0. Each returned string is owned by the caller, who is responsible for releasing it with JSStringRelease.
@param outValues An array that receives up to capacity property values. May be NULL if capacity is 0.
@param capacity The number of entries the out arrays can hold.
@param outCount On success, receives the total number of own enumerable string-keyed properties, which may exceed start + capacity. Pass a capacity of 0 to query the count without reading entries.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result true on success; false if an exception was thrown, in which case the contents of the out arrays are unspecified.
@discussion Entries are returned in the standard property enumeration order, matching Object.entries. Values are read with the object's ordinary get behavior, so getters run. The property set is re-enumerated per call, so a caller reading a large object in windows should expect skipped or repeated entries if a getter mutates the object between calls.
*/
JS_EXPORT bool JSObjectGetOwnEntriesUTF8(JSContextRef ctx, JSObjectRef object, size_t start, JSStringRef* outNames, JSValueRef* outValues, size_t capacity, size_t* outCount, JSValueRef* exception);

/*!
@function
@abstract Defines an accessor (getter/setter) property on an object, using a UTF-8 property name.
@param ctx The execution context to use.
@param object The JSObject on which to define the accessor.
@param name A null-terminated UTF-8 string containing the property name.
@param getter A JSObject that is a function to invoke on property reads. May be NULL to define a write-only accessor.
@param setter A JSObject that is a function to invoke on property writes. May be NULL to define a read-only accessor.
@param attributes A logically ORed set of JSPropertyAttributes to give to the property. kJSPropertyAttributeReadOnly is ignored for accessors.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result true if the accessor was defined; false if an exception was thrown (for example, redefining a non-configurable property, or passing a non-callable getter or setter).
@discussion At least one of getter and setter must be non-NULL.
*/
JS_EXPORT bool JSObjectDefineAccessorUTF8(JSContextRef ctx, JSObjectRef object, const char* name, JSObjectRef getter, JSObjectRef setter, unsigned attributes, JSValueRef* exception);

/*!
@function
@abstract Defines an own data property on an object with the given attributes, equivalent to Object.defineProperty.
@param ctx The execution context to use.
@param object The JSObject on which to define the property.
@param name A null-terminated UTF-8 string containing the property name.
@param value A JSValueRef that is the property value. Pass NULL for the undefined value.
@param attributes A logically ORed set of JSPropertyAttributes to give to the property.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result true if the property was defined; false if the object refused it (for example, redefining a non-configurable property or adding a property to a frozen object).
@discussion Unlike JSObjectSetProperty, this never falls back to ordinary assignment when a
 same-named property exists on the prototype chain: the attributes always apply, and setters
 never run.
*/
JS_EXPORT bool JSObjectDefinePropertyUTF8(JSContextRef ctx, JSObjectRef object, const char* name, JSValueRef value, unsigned attributes, JSValueRef* exception);

/*!
@function
@abstract Freezes an object, equivalent to Object.freeze.
@param ctx The execution context to use.
@param object The JSObject to freeze.
@param exception A pointer to a JSValueRef in which to store an exception, if any. Pass NULL if you do not care to store an exception.
@result true if the object was frozen; false if an exception was thrown (for example, a Proxy that refuses to prevent extensions).
*/
JS_EXPORT bool JSObjectFreeze(JSContextRef ctx, JSObjectRef object, JSValueRef* exception);

/*!
@function
@abstract Creates a JavaScript error object of a given type.
@param ctx The execution context to use.
@param type The JSTypedErrorType of error to create, for example kJSTypedError_TypeError.
@param message A JSString containing the error message. May be NULL to create an error with no message.
@result A JSObject that is an error object of the given type, or NULL if creation failed.
*/
JS_EXPORT JSObjectRef JSObjectMakeTypedError(JSContextRef ctx, JSTypedErrorType type, JSStringRef message);

/*!
@enum JSPromiseStatus
@abstract The possible states of a JavaScript Promise.
@constant kJSPromiseStatus_Pending The promise has not been settled yet.
@constant kJSPromiseStatus_Fulfilled The promise has been resolved with a value.
@constant kJSPromiseStatus_Rejected The promise has been rejected with a reason.
*/
typedef enum {
    kJSPromiseStatus_Pending = 0,
    kJSPromiseStatus_Fulfilled = 1,
    kJSPromiseStatus_Rejected = 2
} JSPromiseStatus;

/*!
@function
@abstract Gets the state of a JavaScript Promise without scheduling any reaction.
@param ctx The execution context to use.
@param promise The JSObject to inspect.
@param outStatus On success, receives the promise's JSPromiseStatus.
@result true if promise is a Promise and outStatus was written; false if promise is not a Promise.
*/
JS_EXPORT bool JSPromiseGetStatus(JSContextRef ctx, JSObjectRef promise, JSPromiseStatus* outStatus);

/*!
@function
@abstract Gets the settled result of a JavaScript Promise: its value if fulfilled, or its rejection reason if rejected.
@param ctx The execution context to use.
@param promise The JSObject to inspect.
@result The promise's result, or NULL if promise is not a Promise. A pending promise yields the undefined value, which is indistinguishable from a promise fulfilled with undefined: consult JSPromiseGetStatus first.
*/
JS_EXPORT JSValueRef JSPromiseGetResult(JSContextRef ctx, JSObjectRef promise);

/*!
@function
@abstract Marks a JavaScript Promise as handled for unhandled-rejection reporting.
@param ctx The execution context to use.
@param promise The JSObject to mark.
@result true if promise is a Promise and was marked; false if promise is not a Promise.
@discussion Use this when native code observes a promise's rejection through the inspection
 functions above instead of attaching a JavaScript reaction, so the page does not report an
 unhandled promise rejection for a rejection that native code in fact handles. Like attaching
 a catch handler, marking only suppresses reports made after it happens.
*/
JS_EXPORT bool JSPromiseMarkAsHandled(JSContextRef ctx, JSObjectRef promise);

/*!
@function
@abstract Reports the amount of non-JavaScript memory kept alive by an object, to guide garbage collection.
@param ctx The execution context to use.
@param object The JSObject the memory is attributed to.
@param bytes The number of bytes of external memory the object keeps alive. Replaces any previously set hint. Pass 0 to clear the hint.
@result true if the hint was stored; false if the object cannot carry a hint, in which case nothing was reported.
@discussion Use this when a small JavaScript wrapper pins a large native allocation, so that the
 collector can account for the true memory cost of the wrapper. The hint is attributed to the
 object for as long as it is alive and is dropped automatically when the object is collected.
 Only objects created with a non-NULL JSClass (for example via JSObjectMake) can carry a hint.
 The hint influences collection scheduling; it is not an exact accounting ledger.
*/
JS_EXPORT bool JSObjectSetExternalMemoryHint(JSContextRef ctx, JSObjectRef object, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* JSObjectRef_h */
