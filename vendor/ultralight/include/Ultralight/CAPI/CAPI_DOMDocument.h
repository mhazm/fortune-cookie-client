///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMDocument.h
///
/// Opaque handles and C functions for inspecting and modifying a page's DOM document.
///
/// `#include <Ultralight/CAPI/CAPI_DOMDocument.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// The C DOM API connects your application to the HTML pages a View loads. It gives you direct
/// access to the page's document so you can update elements and handle user events.
///
/// This example updates an element when a page loads:
///
/// ```c
/// #include <Ultralight/CAPI/CAPI_DOMDocument.h>
/// #include <Ultralight/CAPI/CAPI_DOMElement.h>
///
/// static void OnDOMReady(void* user_data, ULView caller,
///                        unsigned long long frame_id, bool is_main_frame,
///                        ULString url) {
///   if (!is_main_frame)
///     return;
///
///   ULDOMDocument document = ulViewGetDOMDocument(caller);
///   ULDOMElement status = ulDOMDocumentGetElementById(document, "status");
///   ulDOMElementSetTextContent(status, "Connected", 9);
///   ulDOMElementClassListAdd(status, "online", NULL);
///
///   ulDestroyDOMElement(status);
///   ulDestroyDOMDocument(document);
/// }
///
/// void WatchPages(ULView view) {
///   ulViewSetDOMReadyCallback(view, OnDOMReady, NULL, NULL);
/// }
/// ```
///
/// ## Getting the Document
///
/// How you obtain a document depends on whether it belongs to the main frame or a subframe:
///
/// - **Main frame documents come from the View.** Call ulViewGetDOMDocument() inside the DOM-ready
///   callback registered with ulViewSetDOMReadyCallback(). The callback runs for every frame in the
///   View, so check `is_main_frame` to handle only the top-level page.
/// - **Subframe documents come from frame elements.** Call ulDOMElementGetContentDocument() on the
///   `<iframe>` or `<frame>` element.
///
/// This header provides document operations but doesn't include
/// `<Ultralight/CAPI/CAPI_DOMElement.h>`, which you'll need for modifying element attributes,
/// content, and classes.
///
/// ## Where to Start
///
/// Starting functions for common tasks:
///
/// | Task                          | Function                       |
/// |-------------------------------|--------------------------------|
/// | Finding and changing elements | ulDOMDocumentQuerySelector()   |
/// | Listening for events          | ulDOMElementAddEventListener() |
/// | Listeners on every page       | ulCreateDOMTriggers()          |
/// | Viewport and window events    | ulDOMDocumentGetWindow()       |
/// | Text and comment nodes        | ulDOMElementAsNode()           |
///
/// ## Handle Ownership and Comparison
///
/// Rules for managing DOM handles:
///
/// - **Destroy every handle returned by a DOM function.** Call the matching destruction function
///   (such as ulDestroyDOMDocument() or ulDestroyDOMElement()) when you're finished with an owned
///   handle.
/// - **Destroying a handle doesn't remove its node from the page.** Releasing a handle frees only
///   your native reference to the underlying object.
/// - **Callback parameters are borrowed handles.** They stay valid only while the callback runs, so
///   call ulCreateDOMElementRef() or ulCreateDOMDocumentRef() if you need to keep one longer.
/// - **Compare element and node handles with identity functions rather than `==`.** Two lookups of
///   the same node return different pointer addresses, so call ulDOMElementIsSame() or
///   ulDOMNodeIsSame() to check whether two handles refer to the same item.
///
/// ## Handle States Across Navigations
///
/// Every DOM handle is valid (referring to an item on an active page), empty (`NULL`), or gone (its
/// page navigated away, its frame was removed, or its View was destroyed). Calls on a `NULL` handle
/// or a handle whose page is gone fail safely without crashing-- writes do nothing, and reads
/// return default values (such as `0`, `false`, or `NULL`). Destroying these handles is always
/// safe.
///
/// Calling ulDOMDocumentIsAlive() returns `false` for both `NULL` handles and handles whose page is
/// gone. To distinguish between them, check whether the pointer is `NULL` first.
///
/// Once a page navigates away, its handles never become valid again, even if the page returns from
/// the back-forward cache. You'll need to obtain fresh handles for each new page.
///
/// ## Handling Errors
///
/// Functions that can fail take an optional `ULDOMError*` out-parameter to report execution errors.
/// Pass `NULL` if you don't need the error details.
///
/// The library writes to the error struct only when a DOM error occurs, such as a malformed
/// selector or a rejected tree modification. Calls on a `NULL` handle, calls on a handle whose page
/// is gone, and queries that find no matches leave the struct untouched.
///
/// Zero-initialize the `ULDOMError` struct before each call. If `message` is non-`NULL` after a
/// call, you own the string and must release it with ulDestroyString() before reusing or discarding
/// the struct.
///
/// This example checks for errors when querying an element and frees the error message:
///
/// ```c
/// void ShowTotal(ULDOMDocument document) {
///   ULDOMError error = {0};
///   ULDOMElement total =
///       ulDOMDocumentQuerySelector(document, "#scores td.total", &error);
///
///   if (error.code != kULDOMErrorCode_None && error.message) {
///     Log(ulStringGetData(error.message));  // eg, a malformed selector
///     ulDestroyString(error.message);
///   }
///
///   ulDestroyDOMElement(total);  // safe even if it's NULL
/// }
/// ```
///
/// ## Threading Rules
///
/// You must call DOM functions on the Renderer's thread.
///
/// A small group of functions is safe to call from any thread:
///
/// - **Reference, destruction, and status checks.** Calling `ulCreateDOM*Ref()`, `ulDestroyDOM*()`,
///   and `ulDOM*IsAlive()` is safe from background threads.
/// - **Handle conversions and list lookups.** Converting handles (such as ulDOMElementAsNode()),
///   reading list lengths with ulDOMElementListGetLength(), fetching elements with
///   ulDOMElementListGetElement(), and destroying lists with ulDestroyDOMElementList() are
///   thread-safe.
///
/// To inspect or modify the DOM from another thread, post a task to the Renderer's thread with
/// ulRendererPostTask().
///
/// \parblock
/// @note  To locate operations that fail silently, such as a selector with a typo, enable
///        diagnostic logging with ulConfigSetDOMDiagnostics().
/// \endparblock
///
/// \parblock
/// @note  The DOM API works even when JavaScript is disabled through
///        ulViewConfigSetEnableJavaScript().
/// \endparblock
///
/// @see  ulViewGetDOMDocument(), ulViewSetDOMReadyCallback(), ulDOMElementIsSame(),
///       ulConfigSetDOMDiagnostics(), ulRendererPostTask()
///
#ifndef ULTRALIGHT_CAPI_DOMDOCUMENT_H
#define ULTRALIGHT_CAPI_DOMDOCUMENT_H

#include <Ultralight/CAPI/CAPI_Defines.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to a page's DOM document.
///
/// Get the main frame's document with ulViewGetDOMDocument() and a frame's document with
/// ulDOMElementGetContentDocument(). The document's event functions (eg,
/// ulDOMDocumentAddEventListener()) are in `<Ultralight/CAPI/CAPI_DOMEvent.h>`, and its window is
/// in `<Ultralight/CAPI/CAPI_DOMWindow.h>`.
///
/// Every handle to a document is the same handle, so compare two handles to check whether they
/// refer to the same document.
///
/// @see  ulCreateDOMDocumentRef(), ulDestroyDOMDocument()
///
typedef struct C_DOMDocument* ULDOMDocument;

///
/// Opaque handle to a DOM element.
///
/// Two lookups of the same element give you two different handles. Use ulDOMElementIsSame() to
/// check whether two handles refer to the same element.
///
/// @see  ulCreateDOMElementRef(), ulDestroyDOMElement()
///
typedef struct C_DOMElement* ULDOMElement;

///
/// Opaque handle to a DOM node of any kind (an element, text, a comment, and more).
///
/// Every element and every document fragment is also a node. See
/// `<Ultralight/CAPI/CAPI_DOMNode.h>`.
///
/// @see  ulCreateDOMNodeRef(), ulDestroyDOMNode()
///
typedef struct C_DOMNode* ULDOMNode;

///
/// Opaque handle to a list of elements (eg, the result of ulDOMDocumentQuerySelectorAll()).
///
/// The list is a snapshot, so later changes to the page don't change it.
///
/// @see  ulDOMElementListGetLength(), ulDOMElementListGetElement(), ulDestroyDOMElementList()
///
typedef struct C_DOMElementList* ULDOMElementList;

///
/// Opaque handle to a list of nodes of any kind (eg, the result of ulDOMNodeGetChildNodes()).
///
/// The list is a snapshot, so later changes to the page don't change it.
///
/// @see  ulDOMNodeListGetLength(), ulDOMNodeListGetNode(), ulDestroyDOMNodeList()
///
typedef struct C_DOMNodeList* ULDOMNodeList;

///
/// Opaque handle to a document fragment (a container for building nodes off the page and
/// inserting them in one step).
///
/// Every fragment is also a node. Get its node handle with ulDOMFragmentAsNode() to pass it to the
/// node functions (eg, ulDOMElementReplaceChildren() or ulDOMNodeGetChildNodes()). Inserting a
/// fragment, with those functions or with ulDOMElementAppendFragment(), moves all of its children
/// into the page and leaves the fragment empty.
///
/// @see  ulDOMDocumentCreateFragment(), ulDestroyDOMFragment()
///
typedef struct C_DOMFragment* ULDOMFragment;

///
/// Opaque handle to a list of strings (eg, the result of ulDOMElementGetAttributeNames()).
///
/// The list is a snapshot, so later changes to the page don't change it.
///
/// @see  ulDestroyDOMStringList()
///
typedef struct C_DOMStringList* ULDOMStringList;

///
/// Opaque handle to a range (a part of the document between two points, like a JavaScript
/// Range).
///
/// The range keeps its place as the document changes. See `<Ultralight/CAPI/CAPI_DOMRange.h>`.
///
/// @see  ulDOMDocumentCreateRange(), ulDestroyDOMRange()
///
typedef struct C_DOMRange* ULDOMRange;

///
/// Opaque handle to a document's selection (the highlighted text or the caret).
///
/// Reads always return the selection as it is now. See `<Ultralight/CAPI/CAPI_DOMSelection.h>`.
///
/// @see  ulDOMDocumentGetSelection(), ulDestroyDOMSelection()
///
typedef struct C_DOMSelection* ULDOMSelection;

///
/// The kinds of DOM error a ULDOMError can hold (the web's DOMException names, plus TypeError).
///
/// The values match dom::ErrorType in the C++ API. They stay the same across releases. Later
/// versions may add codes, so handle a value you don't know (eg, with a `default` case).
///
typedef enum {
  kULDOMErrorCode_None = 0,                   ///< No DOM error (the zero-initialized state).
  kULDOMErrorCode_Unknown,                    ///< A DOM error with no code of its own here.
  kULDOMErrorCode_SyntaxError,                ///< A malformed selector, or an invalid position
                                              ///< for ulDOMElementInsertAdjacentHTML() and its
                                              ///< siblings.
  kULDOMErrorCode_InvalidCharacterError,      ///< An invalid tag or attribute name.
  kULDOMErrorCode_HierarchyRequestError,      ///< The tree would contain a cycle, or the node
                                              ///< can't go there (eg, text directly under the
                                              ///< document).
  kULDOMErrorCode_NotFoundError,              ///< A node isn't where the call needs it (eg, a
                                              ///< reference child that isn't a child).
  kULDOMErrorCode_NoModificationAllowedError, ///< The target can't be changed (eg, it has no
                                              ///< parent).
  kULDOMErrorCode_InvalidStateError,          ///< The call isn't valid in the target's current
                                              ///< state.
  kULDOMErrorCode_IndexSizeError,             ///< An index or offset is out of range.
  kULDOMErrorCode_NotSupportedError,          ///< The operation isn't supported.
  kULDOMErrorCode_TypeError,                  ///< An argument has the wrong type (eg, a
                                              ///< submitter passed to
                                              ///< ulDOMElementRequestSubmit() that isn't a
                                              ///< submit button).
  kULDOMErrorCode_InvalidNodeTypeError,       ///< The node is the wrong kind for the call (eg,
                                              ///< selecting a node that has no parent).
  kULDOMErrorCode_WrongDocumentError,         ///< A node or range belongs to a different
                                              ///< document.
} ULDOMErrorCode;

///
/// A DOM error (a code and an optional message).
///
/// Functions with a `ULDOMError*` parameter write it only when a DOM error occurs (see the
/// Handling Errors section of CAPI_DOMDocument.h). Zero-initialize it before each call. A DOM
/// error overwrites both fields without freeing the earlier `message`, so before you reuse a
/// ULDOMError, destroy its `message` and zero it again (or the old string leaks).
///
/// @note  If `message` isn't NULL, you own it and must call ulDestroyString() when finished.
///
typedef struct {
  ULDOMErrorCode code;  ///< The kind of error.
  ULString message;     ///< A description of the error (can be NULL).
} ULDOMError;

///
/// Get the document of a View's main frame.
///
/// Call this once the page's DOM is ready (see ulViewSetDOMReadyCallback(), whose callback runs
/// for every frame, so check `is_main_frame`). Get the document again for each page the View
/// loads.
///
/// @param  view  The View (must not be NULL).
///
/// @return  Returns the document, or NULL if the main frame has no document. You must call
///          ulDestroyDOMDocument() when finished.
///
/// \parblock
/// @note  This is the document the frame has now. Before the first page loads, and while a new
///        page is loading, that's the previous document, which goes away when the new page replaces
///        it.
/// \endparblock
///
/// \parblock
/// @note  A page restored from the back-forward cache (see ulConfigSetPageCacheSize()) doesn't
///        run the DOM-ready callback again. Use a restore hook (ulDOMTriggersOnRestore()) to get
///        its document.
/// \endparblock
///
/// \parblock
/// @note  By the time the DOM-ready callback runs, the page's `DOMContentLoaded` event has
///        already fired, so a listener you add then never receives it.
/// \endparblock
///
ULExport ULDOMDocument ulViewGetDOMDocument(ULView view);

///
/// Add a reference to a document handle.
///
/// @param  document  The document handle.
///
/// @return  Returns `document` (NULL if `document` is NULL). You must call ulDestroyDOMDocument()
///          when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMDocument ulCreateDOMDocumentRef(ULDOMDocument document);

///
/// Destroy a document handle (the document itself isn't affected).
///
/// @param  document  The document handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMDocument(ULDOMDocument document);

///
/// Whether or not a document handle is valid (it isn't NULL and its page is still alive).
///
/// @param  document  The document handle.
///
/// @return  Returns true if the document's page is still alive.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMDocumentIsAlive(ULDOMDocument document);

///
/// Find the element with a certain id (getElementById).
///
/// @param  document  The document handle.
///
/// @param  id        The id to look for, as null-terminated UTF-8.
///
/// @return  Returns the element, or NULL if no element has that id. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentGetElementById(ULDOMDocument document, const char* id);

///
/// Find the first element matching a CSS selector (querySelector).
///
/// @param  document   The document handle.
///
/// @param  selectors  A CSS selector list, as null-terminated UTF-8 (eg, `.item > a[href]`).
///
/// @param  error      Where to store a DOM error (can be NULL). This function stores
///                    kULDOMErrorCode_SyntaxError if the selector is malformed.
///
/// @return  Returns the first match in document order, or NULL if nothing matches or the
///          selector is malformed. You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentQuerySelector(ULDOMDocument document, const char* selectors,
                                                 ULDOMError* error);

///
/// Find every element matching a CSS selector (querySelectorAll).
///
/// @param  document   The document handle.
///
/// @param  selectors  A CSS selector list, as null-terminated UTF-8 (eg, `.item > a[href]`).
///
/// @param  error      Where to store a DOM error (can be NULL). This function stores
///                    kULDOMErrorCode_SyntaxError if the selector is malformed.
///
/// @return  Returns a list of the matches in document order (empty if nothing matches), or NULL
///          if the selector is malformed. You must call ulDestroyDOMElementList() when finished.
///
ULExport ULDOMElementList ulDOMDocumentQuerySelectorAll(ULDOMDocument document,
                                                        const char* selectors,
                                                        ULDOMError* error);

///
/// Get the document's root element (documentElement).
///
/// @param  document  The document handle.
///
/// @return  Returns the root element (`<html>` in an HTML document), or NULL if there's none.
///          You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentGetDocumentElement(ULDOMDocument document);

///
/// Get the document's body element (body).
///
/// @param  document  The document handle.
///
/// @return  Returns the `<body>` element, or NULL if there's none. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentGetBody(ULDOMDocument document);

///
/// Get the document's head element (head).
///
/// @param  document  The document handle.
///
/// @return  Returns the `<head>` element, or NULL if there's none. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentGetHead(ULDOMDocument document);

///
/// Get the document's first child of any kind (firstChild).
///
/// @param  document  The document handle.
///
/// @return  Returns the first child (eg, the doctype, a comment, or the root element), or NULL if
///          there's none. You must call ulDestroyDOMNode() when finished.
///
ULExport ULDOMNode ulDOMDocumentGetFirstChild(ULDOMDocument document);

///
/// Get the document's last child of any kind (lastChild).
///
/// @param  document  The document handle.
///
/// @return  Returns the last child, or NULL if there's none. You must call ulDestroyDOMNode() when
///          finished.
///
ULExport ULDOMNode ulDOMDocumentGetLastChild(ULDOMDocument document);

///
/// Get the document's first child element (firstElementChild).
///
/// @param  document  The document handle.
///
/// @return  Returns the first child element (the root element), or NULL if there's none. You must
///          call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentGetFirstElementChild(ULDOMDocument document);

///
/// Get the document's last child element (lastElementChild).
///
/// @param  document  The document handle.
///
/// @return  Returns the last child element (the root element), or NULL if there's none. You must
///          call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentGetLastElementChild(ULDOMDocument document);

///
/// Get the number of the document's child elements (childElementCount).
///
/// @param  document  The document handle.
///
/// @return  Returns the number of child elements (1 for a document with a root element, 0 if it
///          has none).
///
ULExport size_t ulDOMDocumentGetChildElementCount(ULDOMDocument document);

///
/// Get the document's child elements in order (children).
///
/// @param  document  The document handle.
///
/// @return  Returns a list of the child elements (empty if there are none). You must call
///          ulDestroyDOMElementList() when finished.
///
/// @note  Unlike the web's live collection, the list doesn't change when the document does. Call
///        this again to see later changes.
///
ULExport ULDOMElementList ulDOMDocumentGetChildren(ULDOMDocument document);

///
/// Get the document's children of every kind in order (childNodes).
///
/// @param  document  The document handle.
///
/// @return  Returns a list of the child nodes, including the doctype and any comments outside the
///          root element (empty if there are none). You must call ulDestroyDOMNodeList() when
///          finished.
///
/// @note  Unlike the web's live list, the list doesn't change when the document does. Call this
///        again to see later changes.
///
ULExport ULDOMNodeList ulDOMDocumentGetChildNodes(ULDOMDocument document);

///
/// Whether or not the document has any children (hasChildNodes).
///
/// @param  document  The document handle.
///
/// @return  Returns true if the document has at least one child node.
///
ULExport bool ulDOMDocumentHasChildNodes(ULDOMDocument document);

///
/// Create an element (createElement).
///
/// The new element belongs to this document but isn't in the page yet. Add it with
/// ulDOMElementAppendChild() or another insertion function.
///
/// @param  document  The document handle.
///
/// @param  tag_name  The tag name, as null-terminated UTF-8 (eg, `div`). In an HTML document
///                   it's lowercased.
///
/// @param  error     Where to store a DOM error (can be NULL). This function stores
///                   kULDOMErrorCode_InvalidCharacterError if `tag_name` isn't a valid element
///                   name.
///
/// @return  Returns the new element, or NULL if `tag_name` isn't valid. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentCreateElement(ULDOMDocument document, const char* tag_name,
                                                 ULDOMError* error);

///
/// Create a text node (createTextNode).
///
/// The new node belongs to this document but isn't in the page yet. Add it with
/// ulDOMNodeAppendChild() or another insertion function. The text is never parsed as markup.
///
/// @param  document  The document handle.
///
/// @param  text      The text, as UTF-8 (invalid UTF-8 gives an empty text node).
///
/// @param  length    The length of `text`, in bytes.
///
/// @return  Returns the new text node, or NULL if `text` is NULL and `length` isn't 0 (NULL with a
///          length of 0 gives an empty text node). You must call ulDestroyDOMNode() when
///          finished.
///
ULExport ULDOMNode ulDOMDocumentCreateTextNode(ULDOMDocument document, const char* text,
                                               size_t length);

///
/// Create a comment node (createComment).
///
/// The new node belongs to this document but isn't in the page yet. Add it with
/// ulDOMNodeAppendChild() or another insertion function.
///
/// @param  document  The document handle.
///
/// @param  text      The comment's text, as UTF-8 (invalid UTF-8 gives an empty comment).
///
/// @param  length    The length of `text`, in bytes.
///
/// @return  Returns the new comment node, or NULL if `text` is NULL and `length` isn't 0 (NULL
///          with a length of 0 gives an empty comment). You must call ulDestroyDOMNode() when
///          finished.
///
ULExport ULDOMNode ulDOMDocumentCreateComment(ULDOMDocument document, const char* text,
                                              size_t length);

///
/// Create an empty document fragment (createDocumentFragment).
///
/// Build a group of nodes in it with ulDOMFragmentAppend() (or ulDOMFragmentAppendElement() for a
/// single element), then insert them all in one step with ulDOMElementAppendFragment(), or with
/// any node function given its node handle (see ulDOMFragmentAsNode()).
///
/// @param  document  The document handle.
///
/// @return  Returns the new fragment. You must call ulDestroyDOMFragment() when finished.
///
ULExport ULDOMFragment ulDOMDocumentCreateFragment(ULDOMDocument document);

///
/// Create a range (createRange).
///
/// The range starts collapsed at the start of the document. Move it with ulDOMRangeSetStart() or
/// ulDOMRangeSelectNodeContents().
///
/// @param  document  The document handle.
///
/// @return  Returns the new range. You must call ulDestroyDOMRange() when finished.
///
ULExport ULDOMRange ulDOMDocumentCreateRange(ULDOMDocument document);

///
/// Get the document's selection (getSelection).
///
/// The selection is the highlighted text or the caret on the page.
///
/// @param  document  The document handle.
///
/// @return  Returns the selection. You must call ulDestroyDOMSelection() when finished.
///
ULExport ULDOMSelection ulDOMDocumentGetSelection(ULDOMDocument document);

///
/// Add a reference to a fragment handle.
///
/// @param  fragment  The fragment handle.
///
/// @return  Returns `fragment` (NULL if `fragment` is NULL). You must call ulDestroyDOMFragment()
///          when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMFragment ulCreateDOMFragmentRef(ULDOMFragment fragment);

///
/// Destroy a fragment handle (the fragment itself isn't affected).
///
/// @param  fragment  The fragment handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMFragment(ULDOMFragment fragment);

///
/// Whether or not a fragment handle is valid (it isn't NULL and its page is still alive).
///
/// @param  fragment  The fragment handle.
///
/// @return  Returns true if the fragment's page is still alive.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMFragmentIsAlive(ULDOMFragment fragment);

///
/// Get the element that has keyboard focus (activeElement).
///
/// @param  document  The document handle.
///
/// @return  Returns the focused element. When nothing has focus this is the body (NULL if there's
///          no body). You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMDocumentGetActiveElement(ULDOMDocument document);

///
/// Whether or not the document has keyboard focus (hasFocus).
///
/// This is true when the View has focus (see ulViewFocus()) and the focused frame is this
/// document's frame or one of its subframes.
///
/// @param  document  The document handle.
///
/// @return  Returns whether the document has focus (false if the handle's page is gone).
///
ULExport bool ulDOMDocumentHasFocus(ULDOMDocument document);

///
/// Get the document's title (title).
///
/// @param  document  The document handle.
///
/// @return  Returns the text of the `<title>` element with its whitespace collapsed (empty if
///          there's none). You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMDocumentGetTitle(ULDOMDocument document);

///
/// Set the document's title (title).
///
/// This replaces the text of the `<title>` element. If there's no `<title>`, one is added to the
/// `<head>` (nothing happens if there's no `<head>` either).
///
/// @param  document  The document handle.
///
/// @param  title     The title, as UTF-8.
///
/// @param  length    The length of `title`, in bytes.
///
ULExport void ulDOMDocumentSetTitle(ULDOMDocument document, const char* title, size_t length);

///
/// Find the topmost element at a point in the viewport (elementFromPoint).
///
/// @param  document  The document handle.
///
/// @param  x         The horizontal position, in CSS pixels from the viewport's left edge.
///
/// @param  y         The vertical position, in CSS pixels from the viewport's top edge.
///
/// @return  Returns the topmost element at that point, or NULL if the point is outside the
///          viewport. You must call ulDestroyDOMElement() when finished.
///
/// @note  These are the same coordinates as event client positions. To convert from View pixels,
///        divide by the View's device scale (an 800x600 View with a device scale of 2.0 is
///        400x300 CSS pixels).
///
ULExport ULDOMElement ulDOMDocumentElementFromPoint(ULDOMDocument document, double x,
                                                    double y);

///
/// Find every element at a point in the viewport, topmost first (elementsFromPoint).
///
/// The list starts with the element ulDOMDocumentElementFromPoint() returns and continues with
/// the elements painted below it. It ends with the root `<html>` element.
///
/// @param  document  The document handle.
///
/// @param  x         The horizontal position, in CSS pixels from the viewport's left edge.
///
/// @param  y         The vertical position, in CSS pixels from the viewport's top edge.
///
/// @return  Returns a list of the elements (empty if the point is outside the viewport). You must
///          call ulDestroyDOMElementList() when finished.
///
/// @note  These are the same coordinates ulDOMDocumentElementFromPoint() takes. Like that function,
///        this updates the layout first if the page has pending changes.
///
ULExport ULDOMElementList ulDOMDocumentElementsFromPoint(ULDOMDocument document, double x,
                                                         double y);

// The JavaScript handle types, repeated under the shared guard so this header stays
// independent of the ULJS family; they are documented in full in
// <Ultralight/CAPI/CAPI_JSValue.h>.
#ifndef ULJS_HANDLE_TYPES_DEFINED
#define ULJS_HANDLE_TYPES_DEFINED

///
/// Opaque handle to a JavaScript context (see `<Ultralight/CAPI/CAPI_JSValue.h>`).
///
typedef struct C_JSContext* ULJSContext;

///
/// Opaque handle to a JavaScript value (see `<Ultralight/CAPI/CAPI_JSValue.h>`).
///
typedef struct C_JSValue* ULJSValue;

#endif  // ULJS_HANDLE_TYPES_DEFINED

///
/// Get the JavaScript context of a document (the context its page's scripts run in).
///
/// This works for frame documents too (ulViewGetJSContext() gets only the main frame's context).
///
/// @param  document  The document handle.
///
/// @return  Returns the context, or NULL if the page can't run scripts (JavaScript is disabled or
///          the document is sandboxed). You must call ulDestroyJSContext() when finished.
///
/// @see  ulDOMElementGetJSValue()
///
ULExport ULJSContext ulDOMDocumentGetJSContext(ULDOMDocument document);

///
/// Append an element to a fragment (appendChild).
///
/// If the element is already in the page (or in another fragment), it moves here. To add text or
/// several nodes at once, use ulDOMFragmentAppend().
///
/// @param  fragment  The fragment handle.
///
/// @param  element   The element to append.
///
/// @param  error     Where to store a DOM error (can be NULL).
///
/// @return  Returns true if the element was appended.
///
ULExport bool ulDOMFragmentAppendElement(ULDOMFragment fragment, ULDOMElement element,
                                         ULDOMError* error);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMDOCUMENT_H
