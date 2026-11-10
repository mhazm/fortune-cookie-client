///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMElement.h
///
/// Functions for inspecting and modifying DOM elements in C.
///
/// `#include <Ultralight/CAPI/CAPI_DOMElement.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// The DOM element functions let your application inspect and update elements on a page directly
/// from C. You can modify your interface when application state changes, or query element
/// properties to inspect rendered content.
///
/// This function updates an element on the page and modifies an attribute on its parent container:
///
/// ```c
/// void ShowStatus(ULDOMDocument document) {
///   ULDOMElement status = ulDOMDocumentGetElementById(document, "status");
///
///   const char* label = "Connected";
///   ulDOMElementSetTextContent(status, label, strlen(label));
///   ulDOMElementClassListAdd(status, "online", NULL);
///
///   ULDOMElement card = ulDOMElementGetParentElement(status);
///   ulDOMElementSetAttribute(card, "data-state", "online", NULL);
///
///   ulDestroyDOMElement(card);
///   ulDestroyDOMElement(status);
/// }
/// ```
///
/// ## Element Handles
///
/// Every function that returns a ULDOMElement gives you an owned handle, including document
/// queries, element traversals, and list lookups. You must destroy each handle by calling
/// ulDestroyDOMElement() when you're finished with it. Destroying a handle releases only your
/// reference, and the element on the page isn't affected.
///
/// For rules on handle states (valid, empty, or gone), `NULL` handles, error reporting with
/// ULDOMError, and threading, see `<Ultralight/CAPI/CAPI_DOMDocument.h>`.
///
/// ## Snapshot Lists
///
/// Functions that return multiple items return snapshot lists as ULDOMElementList or
/// ULDOMStringList handles. These lists don't change when the document changes, so you'll need to
/// query again to see later changes.
///
/// Each call to ulDOMElementListGetElement() creates a fresh ULDOMElement handle that you own.
/// Destroy each element handle with ulDestroyDOMElement(), and release the list with
/// ulDestroyDOMElementList() when you're finished.
///
/// This function iterates over a list of child elements to update their classes:
///
/// ```c
/// void MarkSeen(ULDOMElement log) {
///   ULDOMElementList items = ulDOMElementQuerySelectorAll(log, "li", NULL);
///   size_t count = ulDOMElementListGetLength(items);
///
///   for (size_t i = 0; i < count; i++) {
///     ULDOMElement item = ulDOMElementListGetElement(items, i);
///     ulDOMElementClassListAdd(item, "seen", NULL);
///     ulDestroyDOMElement(item);
///   }
///
///   ulDestroyDOMElementList(items);
/// }
/// ```
///
/// ## Element-Specific Functions
///
/// Functions designed for specific HTML elements handle unexpected elements and missing data
/// through safe defaults:
///
/// - **Specialized calls accept any element handle.** Functions for form controls, `<select>`
///   elements, and text fields take any ULDOMElement without requiring a separate handle type.
/// - **Incompatible tags return fallback values.** Calling a function on the wrong kind of element
///   returns 0, an empty string, `false`, or `NULL` instead of failing.
/// - **Missing items return sentinels.** A missing attribute or an out-of-range option returns
///   `NULL`, while an unselected option index or unsupported text selection offset returns -1.
///
/// ## Setting Inline Styles
///
/// You can set an inline style property by name in three formats:
///
/// - **Text values accept standard CSS strings.** Call ulDOMElementSetStyleProperty() with a CSS
///   property name and value string (such as `"teal"` or `"12px"`).
/// - **Numeric values take an explicit unit.** Call ulDOMElementSetStylePropertyNumeric() with a
///   ULDOMStyleUnit (such as `kULDOMStyleUnit_Percent` or `kULDOMStyleUnit_Px`) to avoid string
///   formatting.
/// - **Colors pass directly as ULColor.** Call ulDOMElementSetStylePropertyColor() to set a color
///   without string parsing.
///
/// For properties you update every frame, intern the property name once with
/// ulDOMInternStyleProperty() and set the value by ID to skip name lookups.
///
/// This function interns a property ID on its first run and updates an element's width and color:
///
/// ```c
/// static int width_id = 0;
///
/// void ShowHealth(ULDOMElement bar, double health) {
///   if (!width_id)
///     width_id = ulDOMInternStyleProperty("width");
///
///   ulDOMElementSetStylePropertyNumericByID(bar, width_id, health * 100,
///                                           kULDOMStyleUnit_Percent, false);
///   ulDOMElementSetStylePropertyColor(bar, "background-color",
///                                     ulColorRGBA(1.0f, 0.2f, 0.2f, 1.0f),
///                                     false);
/// }
/// ```
///
/// @note  A property ID is valid only in the process that looked it up. Don't share property IDs
///        across processes or save them between application runs.
///
/// @see  `<Ultralight/dom/Element.h>`, ulDOMDocumentQuerySelector(), ulDOMElementAsNode(),
///       ulDOMElementAddEventListener(), ulDOMInternStyleProperty()
///
#ifndef ULTRALIGHT_CAPI_DOMELEMENT_H
#define ULTRALIGHT_CAPI_DOMELEMENT_H

#include <Ultralight/CAPI/CAPI_Color.h>
#include <Ultralight/CAPI/CAPI_DOMDocument.h>
#include <Ultralight/CAPI/CAPI_DOMNode.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Add a reference to an element handle.
///
/// @param  element  The element handle.
///
/// @return  Returns `element` (NULL if `element` is NULL). You must call ulDestroyDOMElement() when
///          finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMElement ulCreateDOMElementRef(ULDOMElement element);

///
/// Destroy an element handle (the element itself isn't affected).
///
/// @param  element  The element handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMElement(ULDOMElement element);

///
/// Whether or not an element handle is valid (it isn't NULL and its page is still alive).
///
/// @param  element  The element handle.
///
/// @return  Returns true if the element's page is still alive (even if the element was removed
///          from the document).
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMElementIsAlive(ULDOMElement element);

///
/// Whether or not two handles refer to the same element (isSameNode).
///
/// @param  element  The first element handle.
///
/// @param  other    The element handle to compare with.
///
/// @return  Returns true if both handles are valid and refer to the same element.
///
ULExport bool ulDOMElementIsSame(ULDOMElement element, ULDOMElement other);

///
/// Get the document this element belongs to (ownerDocument).
///
/// @param  element  The element handle.
///
/// @return  Returns the document of the page this handle came from. You must call
///          ulDestroyDOMDocument() when finished.
///
ULExport ULDOMDocument ulDOMElementGetDocument(ULDOMElement element);

///
/// Get the element's tag name (tagName).
///
/// @param  element  The element handle.
///
/// @return  Returns the tag name (uppercase for HTML elements, eg, `DIV`). You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetTagName(ULDOMElement element);

///
/// Find the first element below this one that matches a CSS selector (querySelector).
///
/// @param  element    The element handle.
///
/// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
///
/// @param  error      Receives kULDOMErrorCode_SyntaxError if the selector is malformed (can be
///                    NULL).
///
/// @return  Returns the first match in document order, or NULL if nothing matches or the selector
///          is malformed. You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementQuerySelector(ULDOMElement element, const char* selectors,
                                                ULDOMError* error);

///
/// Whether or not the element matches a CSS selector (matches).
///
/// @param  element    The element handle.
///
/// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
///
/// @param  error      Receives kULDOMErrorCode_SyntaxError if the selector is malformed (can be
///                    NULL).
///
/// @return  Returns whether the element matches (false if the selector is malformed).
///
ULExport bool ulDOMElementMatches(ULDOMElement element, const char* selectors,
                                  ULDOMError* error);

///
/// Find the closest ancestor that matches a CSS selector, starting with the element itself
/// (closest).
///
/// @param  element    The element handle.
///
/// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
///
/// @param  error      Receives kULDOMErrorCode_SyntaxError if the selector is malformed (can be
///                    NULL).
///
/// @return  Returns the closest match, or NULL if nothing matches or the selector is malformed.
///          You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementClosest(ULDOMElement element, const char* selectors,
                                          ULDOMError* error);

///
/// Get an attribute's value (getAttribute).
///
/// @param  element  The element handle.
///
/// @param  name     The attribute name (eg, `href`). HTML elements ignore its case.
///
/// @return  Returns the value, or NULL if the element doesn't have the attribute. An attribute
///          with no value (eg, `<input disabled>`) returns an empty string. You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetAttribute(ULDOMElement element, const char* name);

///
/// Copy an attribute's value into your own buffer (getAttribute).
///
/// Use this to read a value without creating a ULString. Pass a `capacity` of 0 to get only the
/// length, then call again with a buffer that big.
///
/// @param  element     The element handle.
///
/// @param  name        The attribute name (eg, `href`). HTML elements ignore its case.
///
/// @param  buffer      The buffer that receives the value as UTF-8 (not null-terminated). Can be
///                     NULL if `capacity` is 0.
///
/// @param  capacity    The size of `buffer`, in bytes.
///
/// @param  out_length  Receives the length of the value, in bytes (0 if the element doesn't have
///                     the attribute). Must not be NULL.
///
/// @return  Returns true if the element has the attribute and its value fit in `buffer`. If the
///          value didn't fit, `*out_length` is larger than `capacity`.
///
ULExport bool ulDOMElementGetAttributeUTF8(ULDOMElement element, const char* name,
                                           char* buffer, size_t capacity, size_t* out_length);

///
/// Get an attribute's value as an absolute URL, the way JavaScript reads `src` and `href`.
///
/// A relative value is resolved against the document's base URL, so `src="a.png"` on a page at
/// `https://example.com/games/` reads `https://example.com/games/a.png`.
///
/// @param  element  The element handle.
///
/// @param  name     The attribute name (eg, `src`). HTML elements ignore its case.
///
/// @return  Returns the absolute URL (empty if the element doesn't have the attribute). A value
///          that can't be resolved to a valid URL is returned as it is. You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetAttributeURL(ULDOMElement element, const char* name);

///
/// Set an attribute's value (setAttribute).
///
/// @param  element  The element handle.
///
/// @param  name     The attribute name (eg, `href`). HTML elements store it in lowercase.
///
/// @param  value    The value to set.
///
/// @param  error    Receives kULDOMErrorCode_InvalidCharacterError if `name` isn't a valid
///                  attribute name (can be NULL).
///
/// @return  Returns true if the attribute was set.
///
ULExport bool ulDOMElementSetAttribute(ULDOMElement element, const char* name,
                                       const char* value, ULDOMError* error);

///
/// Whether or not the element has an attribute (hasAttribute).
///
/// @param  element  The element handle.
///
/// @param  name     The attribute name (eg, `href`). HTML elements ignore its case.
///
/// @return  Returns whether the element has the attribute.
///
ULExport bool ulDOMElementHasAttribute(ULDOMElement element, const char* name);

///
/// Remove an attribute (removeAttribute).
///
/// @param  element  The element handle.
///
/// @param  name     The attribute name (eg, `href`).
///
/// @return  Returns true if the attribute was removed (false if the element didn't have it).
///
ULExport bool ulDOMElementRemoveAttribute(ULDOMElement element, const char* name);

///
/// Toggle an attribute (toggleAttribute). This removes the attribute if the element has it, or
/// adds it with an empty value if not.
///
/// @param  element      The element handle.
///
/// @param  name         The attribute name (eg, `hidden`).
///
/// @param  force        A pointer to true to always add the attribute, a pointer to false to
///                      always remove it, or NULL to toggle it.
///
/// @param  out_present  Receives whether the element has the attribute afterward (can be NULL).
///
/// @param  error        Receives kULDOMErrorCode_InvalidCharacterError if `name` isn't a valid
///                      attribute name (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementToggleAttribute(ULDOMElement element, const char* name,
                                          const bool* force, bool* out_present,
                                          ULDOMError* error);

///
/// Get the text of the element and all its descendants (textContent).
///
/// @param  element  The element handle.
///
/// @return  Returns the text. You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetTextContent(ULDOMElement element);

///
/// Copy the text of the element and all its descendants into your own buffer (textContent).
///
/// The buffer works like the one ulDOMElementGetAttributeUTF8() takes.
///
/// @param  element     The element handle.
///
/// @param  buffer      The buffer that receives the text as UTF-8 (not null-terminated). Can be
///                     NULL if `capacity` is 0.
///
/// @param  capacity    The size of `buffer`, in bytes.
///
/// @param  out_length  Receives the length of the text, in bytes. Must not be NULL.
///
/// @return  Returns true if the text fit in `buffer`. If it didn't, `*out_length` is larger than
///          `capacity`.
///
ULExport bool ulDOMElementGetTextContentUTF8(ULDOMElement element, char* buffer,
                                             size_t capacity, size_t* out_length);

///
/// Set the element's text (textContent).
///
/// This replaces the element's children with the text (empty text removes them).
///
/// @param  element  The element handle.
///
/// @param  text     The text, as UTF-8.
///
/// @param  length   The length of `text`, in bytes.
///
ULExport void ulDOMElementSetTextContent(ULDOMElement element, const char* text,
                                         size_t length);

///
/// Find every element below this one that matches a CSS selector (querySelectorAll).
///
/// @param  element    The element handle.
///
/// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
///
/// @param  error      Receives kULDOMErrorCode_SyntaxError if the selector is malformed (can be
///                    NULL).
///
/// @return  Returns a list of the matches in document order (empty if nothing matches), or NULL
///          if the selector is malformed. You must call ulDestroyDOMElementList() when finished.
///
/// @note  The list doesn't change when the document does. Query again to see later changes.
///
ULExport ULDOMElementList ulDOMElementQuerySelectorAll(ULDOMElement element,
                                                       const char* selectors,
                                                       ULDOMError* error);

///
/// Destroy an element list.
///
/// Element handles you got from the list with ulDOMElementListGetElement() aren't affected.
///
/// @param  list  The list (can be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMElementList(ULDOMElementList list);

///
/// Get the number of elements in a list.
///
/// @param  list  The list.
///
/// @return  Returns the number of elements.
///
/// @note  Safe to call from any thread.
///
ULExport size_t ulDOMElementListGetLength(ULDOMElementList list);

///
/// Get an element from a list.
///
/// @param  list   The list.
///
/// @param  index  The index of the element.
///
/// @return  Returns the element, or NULL if `index` is out of range. You must call
///          ulDestroyDOMElement() when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMElement ulDOMElementListGetElement(ULDOMElementList list, size_t index);

///
/// Get the element's parent element (parentElement).
///
/// @param  element  The element handle.
///
/// @return  Returns the parent element, or NULL if the parent isn't an element. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetParentElement(ULDOMElement element);

///
/// Get the element's first child element (firstElementChild).
///
/// @param  element  The element handle.
///
/// @return  Returns the first child element, or NULL if there's none. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetFirstElementChild(ULDOMElement element);

///
/// Get the element's last child element (lastElementChild).
///
/// @param  element  The element handle.
///
/// @return  Returns the last child element, or NULL if there's none. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetLastElementChild(ULDOMElement element);

///
/// Get the element's next sibling element (nextElementSibling).
///
/// @param  element  The element handle.
///
/// @return  Returns the next sibling element, or NULL if there's none. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetNextElementSibling(ULDOMElement element);

///
/// Get the element's previous sibling element (previousElementSibling).
///
/// @param  element  The element handle.
///
/// @return  Returns the previous sibling element, or NULL if there's none. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetPreviousElementSibling(ULDOMElement element);

///
/// Whether or not another element is this element or one of its descendants (contains).
///
/// @param  element  The element handle.
///
/// @param  other    The element to look for.
///
/// @return  Returns true if `other` is `element` or inside it.
///
ULExport bool ulDOMElementContains(ULDOMElement element, ULDOMElement other);

///
/// Get the element's child elements in order (children).
///
/// @param  element  The element handle.
///
/// @return  Returns a list of the child elements (empty if there are none). You must call
///          ulDestroyDOMElementList() when finished.
///
/// @note  Unlike the web's live collection, the list doesn't change when the document does. Call
///        this again to see later changes.
///
ULExport ULDOMElementList ulDOMElementGetChildren(ULDOMElement element);

///
/// Get the number of the element's child elements (childElementCount).
///
/// @param  element  The element handle.
///
/// @return  Returns the number of child elements.
///
ULExport size_t ulDOMElementGetChildElementCount(ULDOMElement element);

///
/// Get the markup of the element's children (innerHTML).
///
/// @param  element  The element handle.
///
/// @return  Returns the markup. You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetInnerHTML(ULDOMElement element);

///
/// Replace the element's children with parsed markup (innerHTML).
///
/// Scripts in the markup never run. Event handler attributes (eg, `onclick`) are kept and work
/// when JavaScript is enabled.
///
/// @param  element  The element handle.
///
/// @param  html     The markup, as UTF-8.
///
/// @param  length   The length of `html`, in bytes.
///
/// @param  error    Receives kULDOMErrorCode_SyntaxError if the markup is malformed in an XML
///                  document (can be NULL). HTML markup always parses.
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementSetInnerHTML(ULDOMElement element, const char* html, size_t length,
                                       ULDOMError* error);

///
/// Get the markup of the element and its children (outerHTML).
///
/// @param  element  The element handle.
///
/// @return  Returns the markup. You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetOuterHTML(ULDOMElement element);

///
/// Replace the element with parsed markup (outerHTML).
///
/// Scripts in the markup never run. Handles to the element still work and refer to the removed
/// element.
///
/// @param  element  The element handle.
///
/// @param  html     The markup, as UTF-8.
///
/// @param  length   The length of `html`, in bytes.
///
/// @param  error    Receives kULDOMErrorCode_NoModificationAllowedError if the element's parent
///                  isn't an element (eg, it's detached or it's the `<html>` root), or
///                  kULDOMErrorCode_SyntaxError if the markup is malformed in an XML document
///                  (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementSetOuterHTML(ULDOMElement element, const char* html, size_t length,
                                       ULDOMError* error);

///
/// Get the element's text as it's rendered (innerText).
///
/// @param  element  The element handle.
///
/// @return  Returns the text. You must call ulDestroyString() when finished.
///
/// @note  This updates the layout first if the page has pending changes. Use
///        ulDOMElementGetTextContent() when you don't need the rendered form.
///
ULExport ULString ulDOMElementGetInnerText(ULDOMElement element);

///
/// Replace the element's children with text (innerText).
///
/// Each line break in the text becomes a `<br>` element. The text is never parsed as markup.
///
/// @param  element  The element handle.
///
/// @param  text     The text, as UTF-8.
///
/// @param  length   The length of `text`, in bytes.
///
/// @note  This does nothing on an element that isn't an HTML element (eg, SVG). Use
///        ulDOMElementSetTextContent() there.
///
ULExport void ulDOMElementSetInnerText(ULDOMElement element, const char* text, size_t length);

///
/// Get the names of the element's attributes in order (getAttributeNames).
///
/// @param  element  The element handle.
///
/// @return  Returns a list of the names (empty if there are none). You must call
///          ulDestroyDOMStringList() when finished.
///
ULExport ULDOMStringList ulDOMElementGetAttributeNames(ULDOMElement element);

///
/// Destroy a string list.
///
/// @param  list  The list (can be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMStringList(ULDOMStringList list);

///
/// Get the number of strings in a list.
///
/// @param  list  The list.
///
/// @return  Returns the number of strings.
///
/// @note  Safe to call from any thread.
///
ULExport size_t ulDOMStringListGetLength(ULDOMStringList list);

///
/// Get a string from a list.
///
/// @param  list   The list.
///
/// @param  index  The index of the string.
///
/// @return  Returns the string, or NULL if `index` is out of range. You must call
///          ulDestroyString() when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULString ulDOMStringListGetString(ULDOMStringList list, size_t index);

///
/// Add an element as the last child of this element (appendChild).
///
/// If `child` is already in a document, it moves from its old position. An element from another
/// document (eg, an iframe's) moves into this one.
///
/// @param  element  The element handle.
///
/// @param  child    The element to add.
///
/// @param  error    Receives kULDOMErrorCode_HierarchyRequestError if `child` is `element` or one
///                  of its ancestors (can be NULL).
///
/// @return  Returns true on success.
///
/// @note  A handle belongs to the page it came from. If you move an element here from another
///        document, its handle still stops working when that document's page goes away.
///
ULExport bool ulDOMElementAppendChild(ULDOMElement element, ULDOMElement child,
                                      ULDOMError* error);

///
/// Insert an element before one of this element's children (insertBefore).
///
/// @param  element    The element handle.
///
/// @param  child      The element to insert. If it's already in a document, it moves.
///
/// @param  ref_child  The child to insert before. Pass NULL to add `child` at the end.
///
/// @param  error      Receives kULDOMErrorCode_NotFoundError if `ref_child` isn't a child of
///                    `element`, or kULDOMErrorCode_HierarchyRequestError if `child` is
///                    `element` or one of its ancestors (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementInsertBefore(ULDOMElement element, ULDOMElement child,
                                       ULDOMElement ref_child, ULDOMError* error);

///
/// Replace one of this element's children with another element (replaceChild).
///
/// @param  element    The element handle.
///
/// @param  new_child  The element to put in its place (it comes first, as on the web). If it's
///                    already in a document, it moves.
///
/// @param  old_child  The child to replace. Its handles still work after it's removed.
///
/// @param  error      Receives kULDOMErrorCode_NotFoundError if `old_child` isn't a child of
///                    `element`, or kULDOMErrorCode_HierarchyRequestError if `new_child` is
///                    `element` or one of its ancestors (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementReplaceChild(ULDOMElement element, ULDOMElement new_child,
                                       ULDOMElement old_child, ULDOMError* error);

///
/// Remove one of this element's children (removeChild).
///
/// @param  element  The element handle.
///
/// @param  child    The child to remove. Its handles still work after it's removed.
///
/// @param  error    Receives kULDOMErrorCode_NotFoundError if `child` isn't a child of
///                  `element` (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementRemoveChild(ULDOMElement element, ULDOMElement child,
                                      ULDOMError* error);

///
/// Remove the element from its parent (remove). Does nothing if it has no parent.
///
/// @param  element  The element handle.
///
/// @param  error    Receives a DOM error if the removal fails (can be NULL).
///
/// @return  Returns true on success (including when the element has no parent).
///
ULExport bool ulDOMElementRemove(ULDOMElement element, ULDOMError* error);

///
/// Add nodes and text to the end of this element, after its last child (append).
///
/// @param  element  The element handle.
///
/// @param  items    The nodes and text to add in order (see ULDOMNodeOrString). A node that's
///                  already in a document moves.
///
/// @param  count    The number of items in `items`.
///
/// @param  error    Receives kULDOMErrorCode_HierarchyRequestError if an item can't go there
///                  (eg, it's `element` or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success.
///
/// @note  With more than one item, the nodes leave their old positions before the insertion is
///        checked (like the web), so a call that fails can leave them out of the page.
///
ULExport bool ulDOMElementAppend(ULDOMElement element, const ULDOMNodeOrString* items,
                                 size_t count, ULDOMError* error);

///
/// Add nodes and text to the start of this element, before its first child (prepend).
///
/// @param  element  The element handle.
///
/// @param  items    The nodes and text to add in order (see ULDOMNodeOrString). A node that's
///                  already in a document moves.
///
/// @param  count    The number of items in `items`.
///
/// @param  error    Receives kULDOMErrorCode_HierarchyRequestError if an item can't go there
///                  (eg, it's `element` or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success.
///
/// @note  With more than one item, the nodes leave their old positions before the insertion is
///        checked (like the web), so a call that fails can leave them out of the page.
///
ULExport bool ulDOMElementPrepend(ULDOMElement element, const ULDOMNodeOrString* items,
                                  size_t count, ULDOMError* error);

///
/// Replace all of this element's children with nodes and text (replaceChildren).
///
/// @param  element  The element handle.
///
/// @param  items    The nodes and text to add in order (see ULDOMNodeOrString). A node that's
///                  already in a document moves. Pass NULL (with a `count` of 0) to remove all
///                  the children.
///
/// @param  count    The number of items in `items`.
///
/// @param  error    Receives kULDOMErrorCode_HierarchyRequestError if an item can't go there
///                  (eg, it's `element` or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success.
///
/// @note  With more than one item, the nodes leave their old positions before the insertion is
///        checked (like the web), so a call that fails can leave them out of the page.
///
ULExport bool ulDOMElementReplaceChildren(ULDOMElement element, const ULDOMNodeOrString* items,
                                          size_t count, ULDOMError* error);

///
/// Copy the element (cloneNode). The copy has the same attributes and isn't in the document.
///
/// @param  element  The element handle.
///
/// @param  deep     True to also copy the element's descendants.
///
/// @return  Returns the copy. You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementCloneNode(ULDOMElement element, bool deep);

///
/// Move all of a fragment's children to the end of this element (appendChild). The fragment is
/// empty afterward.
///
/// @param  element   The element handle.
///
/// @param  fragment  The fragment whose children to move.
///
/// @param  error     Receives a DOM error if the insertion fails (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementAppendFragment(ULDOMElement element, ULDOMFragment fragment,
                                         ULDOMError* error);

///
/// Parse markup and insert it relative to the element (insertAdjacentHTML).
///
/// @param  element   The element handle.
///
/// @param  position  Where to insert. `beforebegin` and `afterend` insert before and after the
///                   element, and `afterbegin` and `beforeend` insert before its first child and
///                   after its last child. Case doesn't matter.
///
/// @param  html      The markup, as UTF-8. Scripts in it never run.
///
/// @param  length    The length of `html`, in bytes.
///
/// @param  error     Receives kULDOMErrorCode_SyntaxError for an unknown `position`, or
///                   kULDOMErrorCode_NoModificationAllowedError for `beforebegin` or `afterend`
///                   when the element has no parent or is the `<html>` root (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementInsertAdjacentHTML(ULDOMElement element, const char* position,
                                             const char* html, size_t length,
                                             ULDOMError* error);

///
/// Insert an element relative to this element (insertAdjacentElement).
///
/// @param  element   The element handle.
///
/// @param  position  Where to insert. `beforebegin` and `afterend` insert before and after the
///                   element, and `afterbegin` and `beforeend` insert before its first child and
///                   after its last child. Case doesn't matter.
///
/// @param  other     The element to insert. If it's already in a document, it moves.
///
/// @param  error     Receives kULDOMErrorCode_SyntaxError for an unknown `position`, or
///                   kULDOMErrorCode_HierarchyRequestError if the insertion isn't allowed (eg,
///                   `other` is an ancestor of `element`) (can be NULL).
///
/// @return  Returns a new handle to `other`, or NULL on failure. It's also NULL (with `error`
///          unchanged) if `position` is `beforebegin` or `afterend` and `element` has no parent,
///          which inserts nothing, like the web. You must call ulDestroyDOMElement() when
///          finished.
///
ULExport ULDOMElement ulDOMElementInsertAdjacentElement(ULDOMElement element,
                                                        const char* position,
                                                        ULDOMElement other, ULDOMError* error);

///
/// Insert text relative to the element (insertAdjacentText).
///
/// The text is never parsed as markup, so this is safe for untrusted text.
///
/// @param  element   The element handle.
///
/// @param  position  Where to insert. `beforebegin` and `afterend` insert before and after the
///                   element, and `afterbegin` and `beforeend` insert before its first child and
///                   after its last child. Case doesn't matter.
///
/// @param  text      The text, as UTF-8.
///
/// @param  length    The length of `text`, in bytes.
///
/// @param  error     Receives kULDOMErrorCode_SyntaxError for an unknown `position`, or
///                   kULDOMErrorCode_HierarchyRequestError for `beforebegin` or `afterend` on
///                   the `<html>` root (can be NULL).
///
/// @return  Returns true on success. For `beforebegin` or `afterend` when the element has no
///          parent, this inserts nothing and still returns true, like the web.
///
ULExport bool ulDOMElementInsertAdjacentText(ULDOMElement element, const char* position,
                                             const char* text, size_t length,
                                             ULDOMError* error);

///
/// The unit of a numeric style value.
///
/// @see  ulDOMElementSetStylePropertyNumeric()
///
typedef enum {
  kULDOMStyleUnit_Number = 0,  ///< A unitless number.
  kULDOMStyleUnit_Percent,     ///< A percentage.
  kULDOMStyleUnit_Px,          ///< Pixels (`px`).
  kULDOMStyleUnit_Em,          ///< Multiples of the element's font size (`em`).
  kULDOMStyleUnit_Rem,         ///< Multiples of the root element's font size (`rem`).
  kULDOMStyleUnit_Vw,          ///< Percent of the viewport's width (`vw`).
  kULDOMStyleUnit_Vh,          ///< Percent of the viewport's height (`vh`).
  kULDOMStyleUnit_Deg,         ///< Degrees (`deg`).
  kULDOMStyleUnit_Ms,          ///< Milliseconds (`ms`).
  kULDOMStyleUnit_S,           ///< Seconds (`s`).
  kULDOMStyleUnit_Empty,       ///< No number (writing it removes the property).
} ULDOMStyleUnit;

///
/// Set an inline style property (style.setProperty).
///
/// @param  element           The element handle.
///
/// @param  name              The property name (eg, `background-color`, or `--accent` for a
///                           custom property).
///
/// @param  value             The value (eg, `teal`). An empty value removes the property.
///
/// @param  important         Whether or not to add `!important`.
///
/// @param  out_parse_failed  Receives whether the value failed to parse (can be NULL).
///
/// @return  Returns true if the value was set. Returns false if the property name is unknown or
///          the value doesn't parse (the old value stays, like the web).
///
/// @note  Custom properties accept any value.
///
ULExport bool ulDOMElementSetStyleProperty(ULDOMElement element, const char* name,
                                           const char* value, bool important,
                                           bool* out_parse_failed);

///
/// Set an inline style property to a number with a unit.
///
/// This saves you formatting the number as a string (eg, for a position or opacity you change
/// every frame).
///
/// @param  element    The element handle.
///
/// @param  name       The property name (eg, `left`). Custom properties aren't supported.
///
/// @param  value      The number (NaN and infinity are ignored).
///
/// @param  unit       The unit of `value`. kULDOMStyleUnit_Empty removes the property.
///
/// @param  important  Whether or not to add `!important`.
///
/// @return  Returns true if the value was set or removed. Returns false if the property name is
///          unknown or custom, `value` isn't finite, or the property doesn't take the value (the
///          old value stays, like the web). The library logs a warning for the last two when
///          DOM diagnostics are on (see ulConfigSetDOMDiagnostics()).
///
/// @note  The value is checked against the property like a string value (eg, kULDOMStyleUnit_Px
///        for `opacity` is ignored).
///
ULExport bool ulDOMElementSetStylePropertyNumeric(ULDOMElement element, const char* name,
                                                  double value, ULDOMStyleUnit unit,
                                                  bool important);

///
/// Set an inline style property to a color.
///
/// This skips string formatting and parsing, so it's the fastest way to change a color every
/// frame.
///
/// @param  element    The element handle.
///
/// @param  name       The property name (eg, `background-color`). A custom property (eg,
///                    `--accent`) gets the color as hex text (eg, `#ff3366`).
///
/// @param  color      The color. An unset color removes the property (like an empty value in
///                    ulDOMElementSetStyleProperty()).
///
/// @param  important  Whether or not to add `!important`.
///
/// @return  Returns true if the value was set or removed. Returns false if the property name is
///          unknown or `color` is invalid (the library logs a warning for an invalid color when
///          DOM diagnostics are on).
///
/// @note  The color isn't checked against the property, so use a property that takes a color.
///
ULExport bool ulDOMElementSetStylePropertyColor(ULDOMElement element, const char* name,
                                                ULColor color, bool important);

///
/// Get the ID of a style property, for the functions that set a property by ID.
///
/// Those functions skip the name lookup that the name-based ones do on every call. Look up each
/// ID once and reuse it.
///
/// @param  name  The property name (eg, `left`).
///
/// @return  Returns the ID, or 0 if the name is unknown or a custom property (use the name-based
///          functions for those).
///
/// @warning  An ID is valid only in the process that looked it up. Never save it or share it
///           with another process or library version.
///
ULExport int ulDOMInternStyleProperty(const char* name);

///
/// Set an inline style property by its ID (like ulDOMElementSetStyleProperty()).
///
/// @param  element           The element handle.
///
/// @param  property_id       An ID from ulDOMInternStyleProperty().
///
/// @param  value             The value (eg, `teal`).
///
/// @param  important         Whether or not to add `!important`.
///
/// @param  out_parse_failed  Receives whether the value failed to parse (can be NULL).
///
/// @return  Returns true if the value was set. Returns false if `property_id` isn't a valid ID or
///          the value doesn't parse (the old value stays, like the web).
///
ULExport bool ulDOMElementSetStylePropertyByID(ULDOMElement element, int property_id,
                                               const char* value, bool important,
                                               bool* out_parse_failed);

///
/// Set an inline style property to a number with a unit by its ID (like
/// ulDOMElementSetStylePropertyNumeric()).
///
/// @param  element      The element handle.
///
/// @param  property_id  An ID from ulDOMInternStyleProperty().
///
/// @param  value        The number (NaN and infinity are ignored).
///
/// @param  unit         The unit of `value`. kULDOMStyleUnit_Empty removes the property.
///
/// @param  important    Whether or not to add `!important`.
///
/// @return  Returns true if the value was set or removed. Returns false if `property_id` isn't a
///          valid ID, `value` isn't finite, or the property doesn't take the value.
///
ULExport bool ulDOMElementSetStylePropertyNumericByID(ULDOMElement element, int property_id,
                                                      double value, ULDOMStyleUnit unit,
                                                      bool important);

///
/// Set an inline style property to a color by its ID (like ulDOMElementSetStylePropertyColor()).
///
/// @param  element      The element handle.
///
/// @param  property_id  An ID from ulDOMInternStyleProperty().
///
/// @param  color        The color. An unset color removes the property.
///
/// @param  important    Whether or not to add `!important`.
///
/// @return  Returns true if the value was set or removed. Returns false if `property_id` isn't a
///          valid ID or `color` is invalid.
///
ULExport bool ulDOMElementSetStylePropertyColorByID(ULDOMElement element, int property_id,
                                                    ULColor color, bool important);

///
/// Get the value of an inline style property (style.getPropertyValue).
///
/// This reads what's set inline on the element, not the value that applies to it. Use
/// ulDOMElementGetComputedStyleProperty() for that.
///
/// @param  element  The element handle.
///
/// @param  name     The property name (eg, `width` or `--accent`).
///
/// @return  Returns the value (empty if the property isn't set inline). You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetStyleProperty(ULDOMElement element, const char* name);

///
/// Remove an inline style property (style.removeProperty).
///
/// @param  element  The element handle.
///
/// @param  name     The property name (eg, `width` or `--accent`).
///
/// @return  Returns true if the property was set inline and is now removed.
///
ULExport bool ulDOMElementRemoveStyleProperty(ULDOMElement element, const char* name);

///
/// Get the element's inline style as CSS text (style.cssText).
///
/// @param  element  The element handle.
///
/// @return  Returns the CSS text (empty if the element has no inline style). You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetStyleCssText(ULDOMElement element);

///
/// Replace the element's inline style with CSS text (style.cssText).
///
/// Declarations that don't parse are dropped, like the web.
///
/// @param  element  The element handle.
///
/// @param  css      The CSS text, as UTF-8 (eg, `color: red; margin: 0`).
///
/// @param  length   The length of `css`, in bytes.
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementSetStyleCssText(ULDOMElement element, const char* css,
                                          size_t length);

///
/// Get the value of a style property that applies to the element
/// (getComputedStyle().getPropertyValue).
///
/// This works for custom properties too.
///
/// @param  element  The element handle.
///
/// @param  name     The property name (eg, `font-size` or `--accent`).
///
/// @return  Returns the value (empty if the property name is unknown). You must call
///          ulDestroyString() when finished.
///
/// @note  This updates the page's style first if it has pending changes, and its layout too for
///        properties that depend on it (eg, `width` or `transform`). Make all your changes first,
///        then read.
///
ULExport ULString ulDOMElementGetComputedStyleProperty(ULDOMElement element,
                                                       const char* name);

///
/// Whether or not the element has a class (classList.contains).
///
/// @param  element  The element handle.
///
/// @param  token    The class name.
///
/// @return  Returns whether the element has the class.
///
ULExport bool ulDOMElementClassListContains(ULDOMElement element, const char* token);

///
/// Add a class to the element (classList.add). Does nothing if the element already has it.
///
/// @param  element  The element handle.
///
/// @param  token    The class name.
///
/// @param  error    Receives kULDOMErrorCode_SyntaxError if `token` is empty, or
///                  kULDOMErrorCode_InvalidCharacterError if it contains whitespace (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementClassListAdd(ULDOMElement element, const char* token,
                                       ULDOMError* error);

///
/// Remove a class from the element (classList.remove). Does nothing if the element doesn't have
/// it.
///
/// @param  element  The element handle.
///
/// @param  token    The class name.
///
/// @param  error    Receives an error as for ulDOMElementClassListAdd() (can be NULL).
///
/// @return  Returns true on success (including when the element didn't have the class).
///
ULExport bool ulDOMElementClassListRemove(ULDOMElement element, const char* token,
                                          ULDOMError* error);

///
/// Toggle a class (classList.toggle). This removes the class if the element has it, or adds it
/// if not.
///
/// @param  element      The element handle.
///
/// @param  token        The class name.
///
/// @param  force        A pointer to true to always add the class, a pointer to false to always
///                      remove it, or NULL to toggle it.
///
/// @param  out_present  Receives whether the element has the class afterward (can be NULL).
///
/// @param  error        Receives an error as for ulDOMElementClassListAdd() (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMElementClassListToggle(ULDOMElement element, const char* token,
                                          const bool* force, bool* out_present,
                                          ULDOMError* error);

///
/// Replace a class with another one in the same position (classList.replace).
///
/// @param  element       The element handle.
///
/// @param  token         The class to replace.
///
/// @param  new_token     The class to put in its place.
///
/// @param  out_replaced  Receives whether the element had `token` (can be NULL).
///
/// @param  error         Receives an error as for ulDOMElementClassListAdd() if either class name
///                       is invalid (can be NULL).
///
/// @return  Returns true on success (including when the element didn't have `token`).
///
ULExport bool ulDOMElementClassListReplace(ULDOMElement element, const char* token,
                                           const char* new_token, bool* out_replaced,
                                           ULDOMError* error);

///
/// Get the number of classes on the element (classList.length).
///
/// @param  element  The element handle.
///
/// @return  Returns the number of classes.
///
ULExport size_t ulDOMElementClassListGetLength(ULDOMElement element);

///
/// Get one of the element's classes by its position (classList.item).
///
/// @param  element  The element handle.
///
/// @param  index    The position of the class in the `class` attribute.
///
/// @return  Returns the class name, or NULL if `index` is out of range. You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementClassListGetItem(ULDOMElement element, size_t index);

///
/// Get the current value of a form control (value).
///
/// This works on `<input>`, `<textarea>`, `<select>`, and `<option>`. An `<option>` without a
/// `value` attribute returns its text, like the web.
///
/// @param  element  The element handle.
///
/// @return  Returns the value (empty for other elements). You must call ulDestroyString() when
///          finished.
///
ULExport ULString ulDOMElementGetValue(ULDOMElement element);

///
/// Set the value of a form control (value).
///
/// This works on `<input>`, `<textarea>`, `<select>` (it selects the option with that value, or
/// clears the selection if none has it), and `<option>` (it sets the `value` attribute).
///
/// @param  element          The element handle.
///
/// @param  value            The value, as UTF-8.
///
/// @param  length           The length of `value`, in bytes.
///
/// @param  dispatch_events  Pass true to fire `input` and `change` events like a user edit. An
///                          `<input>`, `<textarea>`, or `<select>` whose value changed fires
///                          them, both before this returns. Checkbox, radio, hidden, and
///                          button-type inputs and `<option>` never do.
///
/// @param  error            Receives kULDOMErrorCode_InvalidStateError for a non-empty value on a
///                          file input (can be NULL).
///
/// @return  Returns true on success (false for other elements).
///
ULExport bool ulDOMElementSetValue(ULDOMElement element, const char* value, size_t length,
                                   bool dispatch_events, ULDOMError* error);

///
/// Get the default value of an `<input>` or `<textarea>` (defaultValue).
///
/// For an `<input>` this is its `value` attribute. For a `<textarea>` it's the text between its
/// tags. The default value is what the control goes back to when its form is reset.
///
/// @param  element  The element handle.
///
/// @return  Returns the default value (empty for other elements). You must call ulDestroyString()
///          when finished.
///
ULExport ULString ulDOMElementGetDefaultValue(ULDOMElement element);

///
/// Set the default value of an `<input>` or `<textarea>` (defaultValue).
///
/// For an `<input>` this sets its `value` attribute. For a `<textarea>` it replaces the text
/// between its tags. Either way, the current value changes too until the user edits it (or you
/// set it). This fires no events and does nothing on other elements.
///
/// @param  element  The element handle.
///
/// @param  value    The default value, as UTF-8.
///
/// @param  length   The length of `value`, in bytes.
///
ULExport void ulDOMElementSetDefaultValue(ULDOMElement element, const char* value,
                                          size_t length);

///
/// Get the type of an `<input>` (type).
///
/// This is the `type` attribute as the library reads it (always lowercase, and `text` when the
/// attribute is missing or holds a type the library doesn't support).
///
/// Set the `type` attribute to change it.
///
/// @param  element  The element handle.
///
/// @return  Returns the type (eg, `email`), or an empty string for other elements. You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetType(ULDOMElement element);

///
/// Whether or not a checkbox or radio button is checked (checked).
///
/// @param  element  The element handle.
///
/// @return  Returns whether it's checked (false for other elements).
///
ULExport bool ulDOMElementGetChecked(ULDOMElement element);

///
/// Check or uncheck a checkbox or radio button (checked).
///
/// This fires no events, like the web. Use ulDOMElementClick() to toggle it like a user would,
/// firing `input` and `change`.
///
/// @param  element  The element handle.
///
/// @param  checked  True to check it, false to uncheck it.
///
/// @return  Returns true on success (false if the element isn't an `<input>`).
///
ULExport bool ulDOMElementSetChecked(ULDOMElement element, bool checked);

///
/// Whether or not the element has a `disabled` attribute (disabled).
///
/// @param  element  The element handle.
///
/// @return  Returns whether the element has the attribute.
///
/// @note  A control inside a disabled `<fieldset>` is disabled without the attribute. Use
///        ulDOMElementMatches() with `:disabled` to check for that.
///
ULExport bool ulDOMElementGetDisabled(ULDOMElement element);

///
/// Add or remove the element's `disabled` attribute (disabled).
///
/// @param  element   The element handle.
///
/// @param  disabled  True to add the attribute, false to remove it.
///
ULExport void ulDOMElementSetDisabled(ULDOMElement element, bool disabled);

///
/// Get the index of the selected option in a `<select>` (selectedIndex).
///
/// @param  element  The element handle.
///
/// @return  Returns the index, or -1 if no option is selected, the element isn't a `<select>`, or
///          the handle's page is gone (the web returns -1 too).
///
ULExport int64_t ulDOMElementGetSelectedIndex(ULDOMElement element);

///
/// Select an option in a `<select>` by its index (selectedIndex).
///
/// An index that's out of range (including -1) clears the selection. This fires no `change`
/// event, like the web.
///
/// @param  element  The element handle.
///
/// @param  index    The index of the option to select.
///
ULExport void ulDOMElementSetSelectedIndex(ULDOMElement element, int64_t index);

///
/// Get the number of options in a `<select>` (length), including those inside an `<optgroup>`.
///
/// @param  element  The element handle.
///
/// @return  Returns the number of options (0 for other elements).
///
ULExport size_t ulDOMElementGetOptionCount(ULDOMElement element);

///
/// Get an option of a `<select>` by its position (item).
///
/// @param  element  The element handle.
///
/// @param  index    The option's position, counting the options inside an `<optgroup>` too.
///
/// @return  Returns the option, or NULL if `index` is out of range or the element isn't a
///          `<select>`. You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetOption(ULDOMElement element, size_t index);

///
/// Get the selected options of a `<select>` in order (selectedOptions).
///
/// @param  element  The element handle.
///
/// @return  Returns a list of the selected options (empty if none is selected), or NULL if the
///          element isn't a `<select>`. You must call ulDestroyDOMElementList() when finished.
///
/// @note  Unlike the web's live collection, the list doesn't change when the selection does.
///        Call this again to see later changes.
///
ULExport ULDOMElementList ulDOMElementGetSelectedOptions(ULDOMElement element);

///
/// Whether or not an `<option>` is selected (selected).
///
/// This is the current state, not the `selected` attribute.
///
/// @param  element  The element handle.
///
/// @return  Returns whether the option is selected (false for other elements).
///
ULExport bool ulDOMElementGetSelected(ULDOMElement element);

///
/// Select or deselect an `<option>` (selected).
///
/// This fires no `change` event, like the web.
///
/// @param  element   The element handle.
///
/// @param  selected  True to select the option, false to deselect it.
///
ULExport void ulDOMElementSetSelected(ULDOMElement element, bool selected);

///
/// Get an `<option>`'s text with whitespace collapsed (text).
///
/// To set it, call ulDOMElementSetTextContent() (the web's setter does the same).
///
/// @param  element  The element handle.
///
/// @return  Returns the text (empty for other elements). You must call ulDestroyString() when
///          finished.
///
ULExport ULString ulDOMElementGetText(ULDOMElement element);

///
/// Get an `<option>`'s position in its `<select>` (index).
///
/// @param  element  The element handle.
///
/// @return  Returns the position (0 if the option isn't in a `<select>`, like the web, and also
///          for other elements or if the handle's page is gone).
///
ULExport size_t ulDOMElementGetIndex(ULDOMElement element);

///
/// Get the offset where a text control's selection starts (selectionStart).
///
/// The text selection functions work on a `<textarea>` and on the input types that support
/// selection (`text`, `search`, `tel`, `url`, and `password`), like the web. Offsets count
/// UTF-16 code units. The functions that set an offset treat a negative one as 0.
///
/// @param  element  The element handle.
///
/// @return  Returns the offset, or -1 for other input types and elements (the web returns null).
///
ULExport int64_t ulDOMElementGetSelectionStart(ULDOMElement element);

///
/// Get the offset just past the end of a text control's selection (selectionEnd).
///
/// @param  element  The element handle.
///
/// @return  Returns the offset, or -1 as for ulDOMElementGetSelectionStart().
///
ULExport int64_t ulDOMElementGetSelectionEnd(ULDOMElement element);

///
/// Get the direction of a text control's selection (selectionDirection).
///
/// @param  element  The element handle.
///
/// @return  Returns `forward`, `backward`, or `none`, or NULL for other input types and elements
///          (the web returns null). You must call ulDestroyString() when finished.
///
/// @note  By default a selection always has a direction, so `none` reads back as `forward` (see
///        ulViewConfigSetMatchNativeEditingBehavior()).
///
ULExport ULString ulDOMElementGetSelectionDirection(ULDOMElement element);

///
/// Move the start of a text control's selection (selectionStart).
///
/// A value past the end of the text is clamped to it. Changing the selection works as described
/// for ulDOMElementSetSelectionRange().
///
/// @param  element  The element handle.
///
/// @param  start    The offset where the selection starts.
///
/// @param  error    Receives kULDOMErrorCode_InvalidStateError if the input type doesn't support
///                  selection (can be NULL).
///
/// @return  Returns true on success (false if the element isn't a text control).
///
ULExport bool ulDOMElementSetSelectionStart(ULDOMElement element, int64_t start,
                                            ULDOMError* error);

///
/// Move the end of a text control's selection (selectionEnd).
///
/// This works like ulDOMElementSetSelectionStart().
///
/// @param  element  The element handle.
///
/// @param  end      The offset just past the end of the selection.
///
/// @param  error    Receives kULDOMErrorCode_InvalidStateError if the input type doesn't support
///                  selection (can be NULL).
///
/// @return  Returns true on success (false if the element isn't a text control).
///
ULExport bool ulDOMElementSetSelectionEnd(ULDOMElement element, int64_t end,
                                          ULDOMError* error);

///
/// Set the direction of a text control's selection (selectionDirection).
///
/// @param  element    The element handle.
///
/// @param  direction  `forward`, `backward`, or `none`. Anything else sets `none`, like the web.
///
/// @param  error      Receives kULDOMErrorCode_InvalidStateError if the input type doesn't
///                    support selection (can be NULL).
///
/// @return  Returns true on success (false if the element isn't a text control).
///
ULExport bool ulDOMElementSetSelectionDirection(ULDOMElement element, const char* direction,
                                                ULDOMError* error);

///
/// Select all of a text control's text (select).
///
/// This works on a `<textarea>` and on every text field (`email` and `number` too), and does
/// nothing on other elements. Changing the selection works as described for
/// ulDOMElementSetSelectionRange().
///
/// @param  element  The element handle.
///
ULExport void ulDOMElementSelect(ULDOMElement element);

///
/// Set a text control's selection (setSelectionRange).
///
/// Offsets past the end of the text are clamped to it. Setting the selection has two effects
/// besides moving it:
///
///  - **A `select` event fires** during a later ulUpdate() if the selection changed (not during
///    the call).
///  - **A displayed control that doesn't have focus gets keyboard focus** (when the View has
///    focus).
///
/// @param  element    The element handle.
///
/// @param  start      The offset where the selection starts.
///
/// @param  end        The offset just past the end of the selection.
///
/// @param  direction  `forward`, `backward`, or `none` (NULL means `none`).
///
/// @param  error      Receives kULDOMErrorCode_InvalidStateError if the input type doesn't
///                    support selection (can be NULL).
///
/// @return  Returns true on success (false if the element isn't a text control).
///
ULExport bool ulDOMElementSetSelectionRange(ULDOMElement element, int64_t start,
                                            int64_t end, const char* direction,
                                            ULDOMError* error);

///
/// Where the selection goes after ulDOMElementSetRangeTextInRange() replaces text (selectMode).
///
typedef enum {
  kULDOMSelectionMode_Preserve = 0,  ///< Keep the selection, adjusted for the edit.
  kULDOMSelectionMode_Select,        ///< Select the inserted text.
  kULDOMSelectionMode_Start,         ///< Put the caret before the inserted text.
  kULDOMSelectionMode_End,           ///< Put the caret after the inserted text.
} ULDOMSelectionMode;

///
/// Replace the selected text of a text control (setRangeText).
///
/// This fires no `input` or `change` events, like the web.
///
/// @param  element  The element handle.
///
/// @param  text     The text to put in place of the selection, as UTF-8.
///
/// @param  length   The length of `text`, in bytes.
///
/// @param  error    Receives kULDOMErrorCode_InvalidStateError if the input type doesn't support
///                  selection (can be NULL).
///
/// @return  Returns true on success (false if the element isn't a text control).
///
ULExport bool ulDOMElementSetRangeText(ULDOMElement element, const char* text,
                                       size_t length, ULDOMError* error);

///
/// Replace a range of a text control's text (setRangeText with a range).
///
/// Offsets past the end of the text are clamped to it. This fires no `input` or `change`
/// events, like the web.
///
/// @param  element  The element handle.
///
/// @param  text     The text to put in place of the range, as UTF-8.
///
/// @param  length   The length of `text`, in bytes.
///
/// @param  start    The offset where the range starts.
///
/// @param  end      The offset just past the end of the range.
///
/// @param  mode     Where the selection goes afterward.
///
/// @param  error    Receives kULDOMErrorCode_IndexSizeError if `start` is greater than `end`, or
///                  kULDOMErrorCode_InvalidStateError if the input type doesn't support selection
///                  (can be NULL).
///
/// @return  Returns true on success (false if the element isn't a text control).
///
ULExport bool ulDOMElementSetRangeTextInRange(ULDOMElement element, const char* text,
                                              size_t length, int64_t start, int64_t end,
                                              ULDOMSelectionMode mode, ULDOMError* error);

///
/// Submit a form (submit).
///
/// Like the web's form.submit(), this skips validation and fires no `submit` event. It does
/// nothing on other elements.
///
/// @param  element  The element handle.
///
/// @see  ulDOMElementRequestSubmit()
///
ULExport void ulDOMElementSubmit(ULDOMElement element);

///
/// Submit a form like a user would (requestSubmit).
///
/// This checks the form's fields first. If they're valid, it fires a cancelable `submit` event
/// and then submits. Call ulDOMEventPreventDefault() in a `submit` listener to handle the
/// submission natively instead.
///
/// @param  element    The element handle.
///
/// @param  submitter  A submit button in this form to submit with, as if it was clicked (its
///                    attributes, eg, `formaction`, apply). Pass NULL for none.
///
/// @param  error      Receives kULDOMErrorCode_TypeError if `submitter` isn't a submit button, or
///                    kULDOMErrorCode_NotFoundError if it isn't in this form (can be NULL).
///
/// @return  Returns true if the submission was requested, even when invalid fields stop it
///          (false if the element isn't a `<form>`).
///
ULExport bool ulDOMElementRequestSubmit(ULDOMElement element, ULDOMElement submitter,
                                        ULDOMError* error);

///
/// Reset a form's controls to their default values (reset).
///
/// This fires a cancelable `reset` event first. It does nothing on other elements.
///
/// @param  element  The element handle.
///
ULExport void ulDOMElementReset(ULDOMElement element);

///
/// Get the form controls of a `<form>` in document order (elements).
///
/// The list holds the controls that belong to the form, including those outside it whose `form`
/// attribute holds the form's id. Image buttons (`<input type="image">`) aren't included, like
/// the web.
///
/// @param  element  The element handle.
///
/// @return  Returns a list of the controls (empty if there are none), or NULL if the element
///          isn't a `<form>`. You must call ulDestroyDOMElementList() when finished.
///
/// @note  Unlike the web's live collection, the list doesn't change when the form does. Call
///        this again to see later changes.
///
ULExport ULDOMElementList ulDOMElementGetElements(ULDOMElement element);

///
/// Get the form a form control belongs to (form).
///
/// @param  element  The element handle.
///
/// @return  Returns the control's `<form>`, or NULL if it isn't in one or the element isn't a form
///          control. You must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetForm(ULDOMElement element);

///
/// Check whether a form control, or every control in a `<form>`, meets its constraints
/// (checkValidity).
///
/// Constraints come from attributes like `required`, `pattern`, and `maxlength`, from the
/// control's type (eg, an `email` input), and from ulDOMElementSetCustomValidity(). Each invalid
/// control gets a cancelable `invalid` event, which doesn't bubble.
///
/// This works on `<form>` and on form controls like `<input>`, `<textarea>`, `<select>`, and
/// `<button>`. A control excluded from validation (eg, a disabled control or a read-only text
/// field) is always valid.
///
/// @param  element  The element handle.
///
/// @return  Returns true if the control (or every control of the form) is valid, or false if one
///          isn't, the element isn't a form or form control, or the handle's page is gone.
///
/// @see  ulDOMElementGetValidity()
///
ULExport bool ulDOMElementCheckValidity(ULDOMElement element);

///
/// Check whether a form control, or every control in a `<form>`, meets its constraints, and
/// report a failure to the user (reportValidity).
///
/// This works like ulDOMElementCheckValidity(). It also gives keyboard focus to the first invalid
/// control whose `invalid` event wasn't canceled (scrolling it into view).
///
/// @param  element  The element handle.
///
/// @return  Returns true if the control (or every control of the form) is valid, or false if one
///          isn't, the element isn't a form or form control, or the handle's page is gone.
///
/// @note  The library doesn't display a validation message. To show one, read
///        ulDOMElementGetValidationMessage() in an `invalid` listener and display it yourself.
///
ULExport bool ulDOMElementReportValidity(ULDOMElement element);

///
/// Set a custom validation error on a form control (setCustomValidity).
///
/// While the message isn't empty, the control is invalid and its validation message is
/// `message`. Pass an empty message to clear the error.
///
/// @param  element  The element handle.
///
/// @param  message  The error message, as UTF-8.
///
/// @param  length   The length of `message`, in bytes.
///
/// @note  This does nothing on elements that aren't form controls.
///
ULExport void ulDOMElementSetCustomValidity(ULDOMElement element, const char* message,
                                            size_t length);

///
/// Get the message that describes why a form control is invalid (validationMessage).
///
/// @param  element  The element handle.
///
/// @return  Returns the message (eg, the custom message from ulDOMElementSetCustomValidity(), or
///          the library's message for a missing required value), or an empty string if the
///          control is valid, excluded from validation, or isn't a form control. You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMElementGetValidationMessage(ULDOMElement element);

///
/// The ways a form control can fail its constraints (ValidityState).
///
/// @see  ulDOMElementGetValidity()
///
typedef struct {
  bool value_missing;     ///< A `required` control has no value.
  bool type_mismatch;     ///< The value doesn't fit the input type (eg, a malformed email).
  bool pattern_mismatch;  ///< The value doesn't match the `pattern` attribute.
  bool too_long;          ///< The value is longer than `maxlength` (after a user edit).
  bool too_short;         ///< The value is shorter than `minlength` (after a user edit).
  bool range_underflow;   ///< The value is less than `min`.
  bool range_overflow;    ///< The value is greater than `max`.
  bool step_mismatch;     ///< The value doesn't fit the `step` attribute.
  bool bad_input;         ///< The user entered something the control can't convert to a value.
  bool custom_error;      ///< A custom error is set (see ulDOMElementSetCustomValidity()).
  bool valid;             ///< None of the above are true.
} ULDOMValidityState;

///
/// Get how a form control currently fails its constraints (validity).
///
/// @param  element       The element handle.
///
/// @param  out_validity  Receives the control's validity state. It's zeroed first, so every flag
///                       reads false when this returns false. Must not be NULL.
///
/// @return  Returns true if the element is a form control and `out_validity` was filled.
///
/// @note  The state is a copy. Call this again after the value or the constraints change.
///
ULExport bool ulDOMElementGetValidity(ULDOMElement element, ULDOMValidityState* out_validity);

///
/// Click the element (click).
///
/// This fires one `click` event and runs the element's click behavior. For example, a checkbox
/// toggles and fires `input` and `change`, a button activates, and a link navigates. There's no
/// mouse movement and no `mousedown` or `mouseup`, like the web. The event's `isTrusted` is
/// false.
///
/// @param  element  The element handle.
///
/// @note  This does nothing on a disabled form control or a non-HTML element (eg, SVG). Use
///        ulViewFireMouseEvent() to click like a real user.
///
ULExport void ulDOMElementClick(ULDOMElement element);

///
/// Give the element keyboard focus (focus).
///
/// This scrolls the element into view, like the web. It does nothing if the element can't take
/// focus or isn't in the document.
///
/// @param  element  The element handle.
///
ULExport void ulDOMElementFocus(ULDOMElement element);

///
/// Remove keyboard focus from the element (blur). Does nothing if it doesn't have focus.
///
/// @param  element  The element handle.
///
ULExport void ulDOMElementBlur(ULDOMElement element);

///
/// Get the element's position in the keyboard focus order (tabIndex).
///
/// @param  element  The element handle.
///
/// @return  Returns the `tabindex` attribute's value. Without the attribute, this is 0 for
///          elements that take focus by default (eg, links and form controls) and -1 for the
///          rest.
///
ULExport int ulDOMElementGetTabIndex(ULDOMElement element);

///
/// Set the element's position in the keyboard focus order (tabIndex).
///
/// This sets the `tabindex` attribute. Pass -1 to make the element focusable only with
/// ulDOMElementFocus(), not with the Tab key.
///
/// @param  element    The element handle.
///
/// @param  tab_index  The position (0 to follow document order).
///
ULExport void ulDOMElementSetTabIndex(ULDOMElement element, int tab_index);

///
/// A rectangle in CSS pixels (DOMRect).
///
typedef struct {
  double x;       ///< The left edge.
  double y;       ///< The top edge.
  double width;   ///< The width.
  double height;  ///< The height.
} ULDOMRect;

///
/// Get the element's bounding rectangle relative to the viewport (getBoundingClientRect).
///
/// @param  element  The element handle.
///
/// @return  Returns the rectangle in CSS pixels (all zeros if the element isn't displayed). For
///          an element in an iframe, it's relative to the iframe's viewport.
///
/// @note  This updates the layout first if the page has pending changes (so do the offset,
///        client, and scroll reads). Make all your changes first, then read.
///
ULExport ULDOMRect ulDOMElementGetBoundingClientRect(ULDOMElement element);

///
/// Get the element's width in CSS pixels, including its padding and borders (offsetWidth).
///
/// @param  element  The element handle.
///
/// @return  Returns the width in whole pixels (0 if the element isn't displayed).
///
ULExport int ulDOMElementGetOffsetWidth(ULDOMElement element);

///
/// Get the element's height in CSS pixels, including its padding and borders (offsetHeight).
///
/// @param  element  The element handle.
///
/// @return  Returns the height in whole pixels (0 if the element isn't displayed).
///
ULExport int ulDOMElementGetOffsetHeight(ULDOMElement element);

///
/// Get the distance from the element's top border edge to its offset parent's top padding edge,
/// in CSS pixels (offsetTop).
///
/// @param  element  The element handle.
///
/// @return  Returns the distance in whole pixels (0 if the element isn't displayed).
///
/// @see  ulDOMElementGetOffsetParent()
///
ULExport int ulDOMElementGetOffsetTop(ULDOMElement element);

///
/// Get the distance from the element's left border edge to its offset parent's left padding
/// edge, in CSS pixels (offsetLeft).
///
/// @param  element  The element handle.
///
/// @return  Returns the distance in whole pixels (0 if the element isn't displayed).
///
ULExport int ulDOMElementGetOffsetLeft(ULDOMElement element);

///
/// Get the element that ulDOMElementGetOffsetTop() and ulDOMElementGetOffsetLeft() measure from
/// (offsetParent).
///
/// This is the nearest positioned ancestor (or a table cell or table around the element), or the
/// `<body>` if there's none.
///
/// @param  element  The element handle.
///
/// @return  Returns the offset parent, or NULL if the element isn't displayed, has fixed
///          positioning, or is the `<html>` or `<body>` element. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMElementGetOffsetParent(ULDOMElement element);

///
/// Get the element's inner width in CSS pixels, including its padding but not its borders or
/// scrollbar (clientWidth).
///
/// @param  element  The element handle.
///
/// @return  Returns the width in whole pixels (0 if the element isn't displayed or is inline).
///
ULExport int ulDOMElementGetClientWidth(ULDOMElement element);

///
/// Get the element's inner height in CSS pixels, including its padding but not its borders or
/// scrollbar (clientHeight).
///
/// @param  element  The element handle.
///
/// @return  Returns the height in whole pixels (0 if the element isn't displayed or is inline).
///
ULExport int ulDOMElementGetClientHeight(ULDOMElement element);

///
/// Get the width of the element's top border, in CSS pixels (clientTop).
///
/// @param  element  The element handle.
///
/// @return  Returns the width in whole pixels (0 if the element isn't displayed or is inline).
///
ULExport int ulDOMElementGetClientTop(ULDOMElement element);

///
/// Get the width of the element's left border, in CSS pixels (clientLeft).
///
/// @param  element  The element handle.
///
/// @return  Returns the width in whole pixels (0 if the element isn't displayed or is inline).
///
ULExport int ulDOMElementGetClientLeft(ULDOMElement element);

///
/// Get the width of the element's content in CSS pixels, including any part scrolled out of
/// view (scrollWidth).
///
/// @param  element  The element handle.
///
/// @return  Returns the width in whole pixels.
///
ULExport int ulDOMElementGetScrollWidth(ULDOMElement element);

///
/// Get the height of the element's content in CSS pixels, including any part scrolled out of
/// view (scrollHeight).
///
/// @param  element  The element handle.
///
/// @return  Returns the height in whole pixels.
///
ULExport int ulDOMElementGetScrollHeight(ULDOMElement element);

///
/// Get how far the element's content is scrolled right, in CSS pixels (scrollLeft).
///
/// @param  element  The element handle.
///
/// @return  Returns the scroll position (the library currently reports whole pixels).
///
ULExport double ulDOMElementGetScrollLeft(ULDOMElement element);

///
/// Get how far the element's content is scrolled down, in CSS pixels (scrollTop).
///
/// @param  element  The element handle.
///
/// @return  Returns the scroll position (the library currently reports whole pixels).
///
ULExport double ulDOMElementGetScrollTop(ULDOMElement element);

///
/// Scroll the element's content horizontally (scrollLeft).
///
/// This works like ulDOMElementSetScrollTop().
///
/// @param  element  The element handle.
///
/// @param  value    The scroll position in CSS pixels.
///
ULExport void ulDOMElementSetScrollLeft(ULDOMElement element, double value);

///
/// Scroll the element's content vertically (scrollTop).
///
/// The position is clamped to the scrollable range, and scrolling is instant (CSS
/// `scroll-behavior` is ignored). For the root `<html>` element (`<body>` in a quirks-mode
/// page), this scrolls the page.
///
/// @param  element  The element handle.
///
/// @param  value    The scroll position in CSS pixels (any fraction is dropped, and a value that
///                  isn't finite counts as 0).
///
/// @note  This updates the layout first if the page has pending changes.
///
ULExport void ulDOMElementSetScrollTop(ULDOMElement element, double value);

///
/// Scroll the element's ancestors so the element is visible (scrollIntoView).
///
/// Scrolling is instant (CSS `scroll-behavior` is ignored). This does nothing if the element
/// isn't displayed.
///
/// @param  element       The element handle.
///
/// @param  align_to_top  True to line up the element's top with the top of the visible area,
///                       false to line up the bottoms.
///
ULExport void ulDOMElementScrollIntoView(ULDOMElement element, bool align_to_top);

///
/// Scroll the element's content to a position (scrollTo).
///
/// The content scrolls at once (CSS `scroll-behavior` is ignored), and the position stays within
/// the scrollable area. This does nothing if the element isn't displayed or has nothing to scroll.
///
/// @param  element  The element handle.
///
/// @param  x        The horizontal position in CSS pixels (any fraction is dropped). A value that
///                  isn't finite (eg, NaN) counts as 0.
///
/// @param  y        The vertical position in CSS pixels (any fraction is dropped). A value that
///                  isn't finite counts as 0.
///
/// @note  This updates the layout first if the page has pending changes.
///
ULExport void ulDOMElementScrollTo(ULDOMElement element, double x, double y);

///
/// Scroll the element's content by an offset (scrollBy).
///
/// The content scrolls the same way as with ulDOMElementScrollTo().
///
/// @param  element  The element handle.
///
/// @param  x        The horizontal offset in CSS pixels. A value that isn't finite counts as 0.
///
/// @param  y        The vertical offset in CSS pixels. A value that isn't finite counts as 0.
///
ULExport void ulDOMElementScrollBy(ULDOMElement element, double x, double y);

///
/// Get the document inside an `<iframe>` or a legacy `<frame>` (contentDocument).
///
/// @param  element  The element handle.
///
/// @return  Returns the frame's document, or NULL if the element isn't a frame or its frame has no
///          document. This works for a frame from any origin. You must call ulDestroyDOMDocument()
///          when finished.
///
/// @note  The document's handles stop working when the frame navigates or is removed, even while
///        this page lives on.
///
ULExport ULDOMDocument ulDOMElementGetContentDocument(ULDOMElement element);

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
/// Get the element's JavaScript object (the same object the page's scripts see for it).
///
/// Converting one element twice gives the same object (they compare `===`).
///
/// @param  element  The element handle.
///
/// @param  context  The JavaScript context of the element's own frame. Get it with
///                  ulDOMDocumentGetJSContext() (ulViewGetJSContext() gives only the main
///                  frame's context).
///
/// @return  Returns the element's JavaScript object, or NULL if `context` belongs to another frame
///          or View. You must call ulDestroyJSValue() when finished.
///
/// @note  A View with JavaScript disabled has no context, so there's nothing to convert.
///
/// @see  ulDOMElementFromJSValue()
///
ULExport ULJSValue ulDOMElementGetJSValue(ULDOMElement element, ULJSContext context);

///
/// Get the element a JavaScript value refers to (the inverse of ulDOMElementGetJSValue()).
///
/// The element can be in any frame of the value's View, subframes included. The returned handle
/// belongs to the element's own page (not the value's), so it stops working when that page goes
/// away.
///
/// @param  value  A JavaScript value that refers to a DOM element.
///
/// @return  Returns the element, or NULL if `value` isn't a DOM element, the element is in another
///          View, or its document is no longer loaded in a frame. You must call
///          ulDestroyDOMElement() when finished.
///
/// @note  When DOM diagnostics are on, the library logs why it refused an element.
///
/// @see  ulDOMElementGetJSValue()
///
ULExport ULDOMElement ulDOMElementFromJSValue(ULJSValue value);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMELEMENT_H
