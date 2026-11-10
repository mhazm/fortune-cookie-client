///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMRange.h
///
/// Functions for inspecting and modifying spans of page content in C.
///
/// `#include <Ultralight/CAPI/CAPI_DOMRange.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// A ULDOMRange represents a live span of content within a document, bounded by two endpoints. You
/// use ranges to measure rendered text layout or extract portions of the document hierarchy.
///
/// You create a range by calling ulDOMDocumentCreateRange() on a document. For conventions covering
/// handle states, passing NULL handles, error reporting with ULDOMError, and threading rules, see
/// `<Ultralight/CAPI/CAPI_DOMDocument.h>`.
///
/// This function reads an element's text and screen position using a range:
///
/// ```c
/// void ShowQuote(ULDOMDocument document) {
///   ULDOMElement quote = ulDOMDocumentGetElementById(document, "quote");
///   ULDOMNode quote_node = ulDOMElementAsNode(quote);
///   ULDOMRange quote_range = ulDOMDocumentCreateRange(document);
///
///   ulDOMRangeSelectNodeContents(quote_range, quote_node, NULL);
///   ULString quote_text = ulDOMRangeToString(quote_range);
///   ShowTooltip(ulStringGetData(quote_text),
///               ulDOMRangeGetBoundingClientRect(quote_range));
///
///   ulDestroyString(quote_text);
///   ulDestroyDOMRange(quote_range);
///   ulDestroyDOMNode(quote_node);
///   ulDestroyDOMElement(quote);
/// }
/// ```
///
/// ## Boundary Points
///
/// Each boundary point consists of a ULDOMNode handle and an offset within that node. In a text or
/// comment node the offset counts UTF-16 code units. In any other node kind it counts child nodes.
///
/// Endpoints follow these placement rules:
///
/// - **A new range starts at the beginning of the document.** Both boundary points sit in the
///   document itself, where ulDOMRangeGetStartContainer() and ulDOMRangeGetEndContainer() return
///   NULL because a ULDOMDocument isn't a node.
/// - **Boundary nodes must belong to the range's own document.** Passing a node from a different
///   document leaves the boundary unchanged and sets the error code to
///   `kULDOMErrorCode_WrongDocumentError`.
///
/// This function sets boundary points to cover the first five characters of an element's text
/// child:
///
/// ```c
/// void CoverFirstWord(ULDOMRange quote_range, ULDOMNode quote_node) {
///   ULDOMNode first_text = ulDOMNodeGetFirstChild(quote_node);
///   ulDOMRangeSetStart(quote_range, first_text, 0, NULL);
///   ulDOMRangeSetEnd(quote_range, first_text, 5, NULL);
///   ulDestroyDOMNode(first_text);
/// }
/// ```
///
/// ## Live Updates
///
/// A range updates automatically as the page changes. When nodes are inserted or removed, the
/// library moves the boundary points to match (if a boundary node is removed from the document, its
/// point moves to where that node sat in its parent).
///
/// Each live range adds a little work to every DOM change in its document, so destroy ranges with
/// ulDestroyDOMRange() once you're done with them.
///
/// ## Copying Ranges
///
/// You can duplicate a range handle in two ways:
///
/// - **Calling ulCreateDOMRangeRef() adds another reference to the same range.** Modifications made
///   through either handle affect both, and you must destroy each handle with ulDestroyDOMRange().
/// - **Calling ulDOMRangeCloneRange() creates an independent copy.** The cloned range updates its
///   boundary points on its own, and you must destroy it with ulDestroyDOMRange().
///
/// @see  `<Ultralight/dom/Range.h>`, ulDOMDocumentCreateRange(), ulDOMRangeSelectNodeContents(),
///       ulDOMRangeCloneRange(), ulDOMSelectionAddRange()
///
#ifndef ULTRALIGHT_CAPI_DOMRANGE_H
#define ULTRALIGHT_CAPI_DOMRANGE_H

#include <Ultralight/CAPI/CAPI_DOMElement.h>
#include <Ultralight/CAPI/CAPI_DOMNode.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Which boundary points ulDOMRangeCompareBoundaryPoints() compares (the web's
/// Range.START_TO_START constants).
///
typedef enum {
  kULDOMRangeCompareHow_StartToStart = 0,  ///< Compare the two start points.
  kULDOMRangeCompareHow_StartToEnd = 1,    ///< This end vs the other's start.
  kULDOMRangeCompareHow_EndToEnd = 2,      ///< Compare the two end points.
  kULDOMRangeCompareHow_EndToStart = 3,    ///< This start vs the other's end.
} ULDOMRangeCompareHow;

///
/// Add a reference to a range handle.
///
/// @param  range  The range handle.
///
/// @return  Returns `range` (NULL if `range` is NULL). You must call ulDestroyDOMRange() when
///          finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMRange ulCreateDOMRangeRef(ULDOMRange range);

///
/// Destroy a range handle (the page isn't affected).
///
/// @param  range  The range handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMRange(ULDOMRange range);

///
/// Whether or not a range handle is valid (it isn't NULL and its page is still alive).
///
/// @param  range  The range handle.
///
/// @return  Returns true if the range's page is still alive.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMRangeIsAlive(ULDOMRange range);

///
/// Get the node that holds the range's start (startContainer).
///
/// @param  range  The range handle.
///
/// @return  Returns the node, or NULL if the start is in the document itself. You must call
///          ulDestroyDOMNode() when finished.
///
ULExport ULDOMNode ulDOMRangeGetStartContainer(ULDOMRange range);

///
/// Get the offset of the range's start in its node (startOffset).
///
/// @param  range  The range handle.
///
/// @return  Returns the offset.
///
ULExport size_t ulDOMRangeGetStartOffset(ULDOMRange range);

///
/// Get the node that holds the range's end (endContainer).
///
/// @param  range  The range handle.
///
/// @return  Returns the node, or NULL if the end is in the document itself. You must call
///          ulDestroyDOMNode() when finished.
///
ULExport ULDOMNode ulDOMRangeGetEndContainer(ULDOMRange range);

///
/// Get the offset of the range's end in its node (endOffset).
///
/// @param  range  The range handle.
///
/// @return  Returns the offset.
///
ULExport size_t ulDOMRangeGetEndOffset(ULDOMRange range);

///
/// Whether or not the range's start and end are the same point (collapsed).
///
/// @param  range  The range handle.
///
/// @return  Returns true if the range is collapsed (also for a NULL handle or one whose page is
///          gone).
///
ULExport bool ulDOMRangeGetCollapsed(ULDOMRange range);

///
/// Get the deepest node that contains both boundary points (commonAncestorContainer).
///
/// @param  range  The range handle.
///
/// @return  Returns the node, or NULL if it's the document itself. You must call
///          ulDestroyDOMNode() when finished.
///
ULExport ULDOMNode ulDOMRangeGetCommonAncestorContainer(ULDOMRange range);

///
/// Set the range's start (setStart).
///
/// If the new start is after the range's end, the range collapses to the new start.
///
/// @param  range   The range handle.
///
/// @param  node    The node for the new start (from the range's document).
///
/// @param  offset  The offset in `node` (see Boundary Points above).
///
/// @param  error   Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_IndexSizeError if `offset` is past the end of `node`,
///          kULDOMErrorCode_InvalidNodeTypeError if `node` is a doctype, or
///          kULDOMErrorCode_WrongDocumentError if `node` is from another document.
///
ULExport bool ulDOMRangeSetStart(ULDOMRange range, ULDOMNode node, size_t offset,
                                 ULDOMError* error);

///
/// Set the range's end (setEnd).
///
/// If the new end is before the range's start, the range collapses to the new end.
///
/// @param  range   The range handle.
///
/// @param  node    The node for the new end (from the range's document).
///
/// @param  offset  The offset in `node` (see Boundary Points above).
///
/// @param  error   Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_IndexSizeError if `offset` is past the end of `node`,
///          kULDOMErrorCode_InvalidNodeTypeError if `node` is a doctype, or
///          kULDOMErrorCode_WrongDocumentError if `node` is from another document.
///
ULExport bool ulDOMRangeSetEnd(ULDOMRange range, ULDOMNode node, size_t offset,
                               ULDOMError* error);

///
/// Collapse the range to its start or its end (collapse).
///
/// @param  range     The range handle.
///
/// @param  to_start  Pass true to collapse to the start and false to collapse to the end.
///
ULExport void ulDOMRangeCollapse(ULDOMRange range, bool to_start);

///
/// Make the range cover a whole node (selectNode).
///
/// The range starts just before `node` and ends just after it, in `node`'s parent.
///
/// @param  range  The range handle.
///
/// @param  node   The node to cover (from the range's document).
///
/// @param  error  Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_InvalidNodeTypeError if `node` has no parent, or
///          kULDOMErrorCode_WrongDocumentError if `node` is from another document.
///
ULExport bool ulDOMRangeSelectNode(ULDOMRange range, ULDOMNode node, ULDOMError* error);

///
/// Make the range cover everything inside a node (selectNodeContents).
///
/// For an element that's all of its children. For a text or comment node it's all of its text.
///
/// @param  range  The range handle.
///
/// @param  node   The node whose contents to cover (from the range's document).
///
/// @param  error  Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_InvalidNodeTypeError if `node` is a doctype, or
///          kULDOMErrorCode_WrongDocumentError if `node` is from another document.
///
ULExport bool ulDOMRangeSelectNodeContents(ULDOMRange range, ULDOMNode node,
                                           ULDOMError* error);

///
/// Compare a boundary point of a range with one of another range (compareBoundaryPoints).
///
/// @param  range       The range handle.
///
/// @param  how         Which point of each range to compare (see ULDOMRangeCompareHow).
///
/// @param  other       The range to compare with.
///
/// @param  out_result  Receives -1, 0, or 1 if the point of `range` is before, at, or after the
///                     point of `other` (can be NULL).
///
/// @param  error       Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_WrongDocumentError if the two points aren't in the same tree (eg,
///          the ranges are in different documents).
///
ULExport bool ulDOMRangeCompareBoundaryPoints(ULDOMRange range, ULDOMRangeCompareHow how,
                                              ULDOMRange other, int* out_result,
                                              ULDOMError* error);

///
/// Remove the range's contents from the page (deleteContents).
///
/// Text at the edges of the range is trimmed, and the range collapses to where the contents were.
///
/// @param  range  The range handle.
///
/// @param  error  Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMRangeDeleteContents(ULDOMRange range, ULDOMError* error);

///
/// Move the range's contents out of the page into a new fragment (extractContents).
///
/// A node that's only partly inside the range stays in the page without the covered part, and
/// the fragment gets a copy that holds that part. The range collapses to where the contents were.
///
/// @param  range  The range handle.
///
/// @param  error  Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns the fragment. You must call ulDestroyDOMFragment() when finished. Returns
///          NULL and sets `error` to kULDOMErrorCode_HierarchyRequestError if the range contains
///          a doctype.
///
ULExport ULDOMFragment ulDOMRangeExtractContents(ULDOMRange range, ULDOMError* error);

///
/// Copy the range's contents into a new fragment (cloneContents). The page doesn't change.
///
/// @param  range  The range handle.
///
/// @param  error  Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns the fragment. You must call ulDestroyDOMFragment() when finished. Returns
///          NULL and sets `error` to kULDOMErrorCode_HierarchyRequestError if the range contains
///          a doctype.
///
ULExport ULDOMFragment ulDOMRangeCloneContents(ULDOMRange range, ULDOMError* error);

///
/// Insert a node at the range's start (insertNode).
///
/// If the start is inside a text node, the text node is split there. If the range is collapsed,
/// it grows to cover the inserted node.
///
/// @param  range  The range handle.
///
/// @param  node   The node to insert (from the range's document). If it already has a parent,
///                it's moved.
///
/// @param  error  Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_HierarchyRequestError if `node` can't go there (eg, the start is
///          inside a comment, or `node` contains the start), or
///          kULDOMErrorCode_WrongDocumentError if `node` is from another document.
///
ULExport bool ulDOMRangeInsertNode(ULDOMRange range, ULDOMNode node, ULDOMError* error);

///
/// Create an independent copy of a range (cloneRange).
///
/// @param  range  The range handle.
///
/// @return  Returns a new range with the same boundary points (NULL for a NULL handle or one
///          whose page is gone). From then on, each range updates on its own. You must call
///          ulDestroyDOMRange() when finished.
///
ULExport ULDOMRange ulDOMRangeCloneRange(ULDOMRange range);

///
/// Get the text the range covers (toString).
///
/// @param  range  The range handle.
///
/// @return  Returns the text of the text nodes in the range (comments aren't included). You must
///          call ulDestroyString() when finished.
///
ULExport ULString ulDOMRangeToString(ULDOMRange range);

///
/// Get the smallest rectangle that contains everything the range covers (getBoundingClientRect).
///
/// @param  range  The range handle.
///
/// @return  Returns the rectangle in the same viewport coordinates as
///          ulDOMElementGetBoundingClientRect() (all zero if nothing in the range is displayed).
///
/// @note  This reads the layout, so it forces a synchronous style and layout pass if the document
///        has pending changes.
///
ULExport ULDOMRect ulDOMRangeGetBoundingClientRect(ULDOMRange range);

///
/// Get a rectangle for each box the range covers (getClientRects).
///
/// Text that wraps gives one rectangle per line. An element that's fully inside the range adds
/// its own border box too. The rectangles are in document order, in the same viewport
/// coordinates as ulDOMElementGetBoundingClientRect().
///
/// To get them all, call this once with a `capacity` of 0 to get the count, then again with an
/// array that big.
///
/// @param  range     The range handle.
///
/// @param  rects     The array to fill (can be NULL when `capacity` is 0).
///
/// @param  capacity  The number of rectangles `rects` can hold. At most this many are written.
///
/// @return  Returns the total number of rectangles, even if it's more than `capacity`.
///
/// @note  This reads the layout, so it forces a synchronous style and layout pass if the document
///        has pending changes.
///
ULExport size_t ulDOMRangeGetClientRects(ULDOMRange range, ULDOMRect* rects,
                                         size_t capacity);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMRANGE_H
