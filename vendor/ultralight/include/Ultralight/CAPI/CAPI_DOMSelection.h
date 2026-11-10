///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMSelection.h
///
/// Functions for inspecting and modifying the active user selection or caret in a document.
///
/// `#include <Ultralight/CAPI/CAPI_DOMSelection.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// A ULDOMSelection handle represents either a caret or highlighted text on a page. You obtain one
/// for a document by calling ulDOMDocumentGetSelection(), and destroy it with
/// ulDestroyDOMSelection() when finished.
///
/// This example checks whether the user highlighted text and shows a quote button:
///
/// ```c
/// void UpdateQuoteButton(ULDOMDocument document) {
///   ULDOMSelection selection = ulDOMDocumentGetSelection(document);
///
///   if (!ulDOMSelectionIsCollapsed(selection)) {
///     ULString selected = ulDOMSelectionToString(selection);
///     ShowQuoteButton(ulStringGetData(selected));
///     ulDestroyString(selected);
///   }
///
///   ulDestroyDOMSelection(selection);
/// }
/// ```
///
/// ## Anchor and Focus
///
/// A selection spans between endpoints in the document tree:
///
/// - **The anchor marks where the selection starts.** Calling ulDOMSelectionGetAnchorNode() returns
///   the containing ULDOMNode (or NULL when nothing is selected), and
///   ulDOMSelectionGetAnchorOffset() returns the offset within that node.
/// - **The focus marks where the selection ends.** Calling ulDOMSelectionGetFocusNode() and
///   ulDOMSelectionGetFocusOffset() returns its position, which can sit before the anchor in the
///   document when the user selects backward.
///
/// You must destroy any non-NULL node handle with ulDestroyDOMNode(). Each document has its own
/// selection (including each subframe), read live through any ULDOMSelection handle.
///
/// ## Reacting to Selection Changes
///
/// Every change to the active selection dispatches a `selectionchange` event on the document. You
/// listen for it with ulDOMDocumentAddEventListener(), and the library delivers these events during
/// a later ulUpdate(), dispatching one event per change.
///
/// ## Selecting a Range
///
/// To select a ULDOMRange, call ulDOMSelectionRemoveAllRanges() before calling
/// ulDOMSelectionAddRange(). On its own, ulDOMSelectionAddRange() only expands an existing
/// selection if the new range overlaps it, and it ignores any range that doesn't overlap.
///
/// This example selects the contents of a node:
///
/// ```c
/// void SelectQuote(ULDOMSelection selection, ULDOMDocument document,
///                  ULDOMNode quote_node) {
///   ULDOMRange quote_range = ulDOMDocumentCreateRange(document);
///   ulDOMRangeSelectNodeContents(quote_range, quote_node, NULL);
///
///   ulDOMSelectionRemoveAllRanges(selection);
///   ulDOMSelectionAddRange(selection, quote_range);
///   ulDestroyDOMRange(quote_range);
/// }
/// ```
///
/// ## Selections in Text Fields
///
/// The selection doesn't enter editable form controls. When an `<input>` or `<textarea>` has focus,
/// the nodes returned by ulDOMSelectionGetAnchorNode() and ulDOMSelectionGetFocusNode() sit just
/// outside the element, and ulDOMSelectionIsCollapsed() returns true.
///
/// To read or change selected text inside an editable field, call ulDOMElementGetSelectionStart()
/// and ulDOMElementGetSelectionEnd() on the element instead (declared in
/// `<Ultralight/CAPI/CAPI_DOMElement.h>`).
///
/// ## Differences from the Web
///
/// Several functions handle node validation and range ownership differently from web browsers:
///
/// - **Nodes from other documents are ignored.** Passing a node from another document (such as a
///   subframe) leaves the selection unchanged, and the call still returns true.
/// - **Offsets past the end of a node fail only in ulDOMSelectionExtend().** Other placement
///   functions like ulDOMSelectionCollapse() and ulDOMSelectionSetBaseAndExtent() don't check
///   whether an offset exceeds the node's length.
/// - **Range functions work with independent copies.** Calling ulDOMSelectionGetRangeAt() returns a
///   new ULDOMRange that you must destroy with ulDestroyDOMRange(), and ulDOMSelectionAddRange()
///   copies the boundary points of the passed range so modifying it later doesn't alter the active
///   selection.
///
/// For rules on handle states, NULL handles, DOM errors, and calling threads, see
/// `<Ultralight/CAPI/CAPI_DOMDocument.h>`.
///
/// @see  `<Ultralight/dom/Selection.h>`, ulDOMDocumentGetSelection(), ulDOMSelectionAddRange(),
///       ulDOMSelectionGetRangeAt(), ulDOMElementGetSelectionStart()
///
#ifndef ULTRALIGHT_CAPI_DOMSELECTION_H
#define ULTRALIGHT_CAPI_DOMSELECTION_H

#include <Ultralight/CAPI/CAPI_DOMNode.h>
#include <Ultralight/CAPI/CAPI_DOMRange.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Add a reference to a selection handle.
///
/// @param  selection  The selection handle.
///
/// @return  Returns `selection` (NULL if `selection` is NULL). You must call
///          ulDestroyDOMSelection() when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMSelection ulCreateDOMSelectionRef(ULDOMSelection selection);

///
/// Destroy a selection handle (the selection itself isn't affected).
///
/// @param  selection  The selection handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMSelection(ULDOMSelection selection);

///
/// Whether or not a selection handle is valid (it isn't NULL and its page is still alive).
///
/// @param  selection  The selection handle.
///
/// @return  Returns true if the selection's page is still alive.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMSelectionIsAlive(ULDOMSelection selection);

///
/// Get the node the selection starts in (anchorNode).
///
/// @param  selection  The selection handle.
///
/// @return  Returns the node, or NULL if nothing is selected. You must call ulDestroyDOMNode()
///          when finished.
///
ULExport ULDOMNode ulDOMSelectionGetAnchorNode(ULDOMSelection selection);

///
/// Get the offset of the anchor in its node (anchorOffset).
///
/// @param  selection  The selection handle.
///
/// @return  Returns the offset.
///
ULExport size_t ulDOMSelectionGetAnchorOffset(ULDOMSelection selection);

///
/// Get the node the selection ends in (focusNode).
///
/// @param  selection  The selection handle.
///
/// @return  Returns the node, or NULL if nothing is selected. You must call ulDestroyDOMNode()
///          when finished.
///
ULExport ULDOMNode ulDOMSelectionGetFocusNode(ULDOMSelection selection);

///
/// Get the offset of the focus in its node (focusOffset).
///
/// @param  selection  The selection handle.
///
/// @return  Returns the offset.
///
ULExport size_t ulDOMSelectionGetFocusOffset(ULDOMSelection selection);

///
/// Whether or not the selection is a caret (isCollapsed).
///
/// @param  selection  The selection handle.
///
/// @return  Returns true for a caret, and also when nothing is selected (or the handle is NULL or
///          its page is gone).
///
ULExport bool ulDOMSelectionIsCollapsed(ULDOMSelection selection);

///
/// Get the number of ranges in the selection (rangeCount).
///
/// @param  selection  The selection handle.
///
/// @return  Returns 0 if nothing is selected, otherwise 1.
///
ULExport size_t ulDOMSelectionGetRangeCount(ULDOMSelection selection);

///
/// Get the kind of selection (type).
///
/// @param  selection  The selection handle.
///
/// @return  Returns `None` (nothing is selected), `Caret`, or `Range`. You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMSelectionGetType(ULDOMSelection selection);

///
/// Get the selected text (toString).
///
/// @param  selection  The selection handle.
///
/// @return  Returns the text. You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMSelectionToString(ULDOMSelection selection);

///
/// Place a caret (collapse or setPosition).
///
/// @param  selection  The selection handle.
///
/// @param  node       The node to put the caret in. Pass NULL to clear the selection.
///
/// @param  offset     The offset in `node` (like a range boundary point).
///
/// @param  error      Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success.
///
/// @note  Unlike the web, an `offset` past the end of `node` doesn't fail, so check it yourself.
///
ULExport bool ulDOMSelectionCollapse(ULDOMSelection selection, ULDOMNode node,
                                     size_t offset, ULDOMError* error);

///
/// Collapse the selection to its start (collapseToStart).
///
/// @param  selection  The selection handle.
///
/// @param  error      Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_InvalidStateError if nothing is selected.
///
ULExport bool ulDOMSelectionCollapseToStart(ULDOMSelection selection, ULDOMError* error);

///
/// Collapse the selection to its end (collapseToEnd).
///
/// @param  selection  The selection handle.
///
/// @param  error      Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_InvalidStateError if nothing is selected.
///
ULExport bool ulDOMSelectionCollapseToEnd(ULDOMSelection selection, ULDOMError* error);

///
/// Move the focus and keep the anchor where it is (extend).
///
/// Use this to grow or shrink the selection from one end.
///
/// @param  selection  The selection handle.
///
/// @param  node       The node to move the focus into.
///
/// @param  offset     The offset in `node` (like a range boundary point).
///
/// @param  error      Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success. Returns false and sets `error` to
///          kULDOMErrorCode_InvalidStateError if nothing is selected, or
///          kULDOMErrorCode_IndexSizeError if `offset` is past the end of `node`.
///
ULExport bool ulDOMSelectionExtend(ULDOMSelection selection, ULDOMNode node,
                                   size_t offset, ULDOMError* error);

///
/// Set the anchor and the focus in one call (setBaseAndExtent).
///
/// The focus can come before the anchor in the document.
///
/// @param  selection      The selection handle.
///
/// @param  anchor_node    The node for the anchor.
///
/// @param  anchor_offset  The offset in `anchor_node`.
///
/// @param  focus_node     The node for the focus.
///
/// @param  focus_offset   The offset in `focus_node`.
///
/// @param  error          Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success.
///
/// \parblock
/// @note  If one node is NULL, the selection becomes a caret at the other point. If both are
///        NULL, the selection is cleared.
/// \endparblock
///
/// \parblock
/// @note  Unlike the web, an offset past the end of its node doesn't fail, so check offsets
///        yourself.
/// \endparblock
///
ULExport bool ulDOMSelectionSetBaseAndExtent(ULDOMSelection selection,
                                             ULDOMNode anchor_node,
                                             size_t anchor_offset,
                                             ULDOMNode focus_node,
                                             size_t focus_offset, ULDOMError* error);

///
/// Select everything inside a node (selectAllChildren).
///
/// @param  selection  The selection handle.
///
/// @param  node       The node whose children to select. A text node has no children, so this
///                    places a caret at its start.
///
/// @param  error      Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMSelectionSelectAllChildren(ULDOMSelection selection, ULDOMNode node,
                                              ULDOMError* error);

///
/// Clear the selection (removeAllRanges or empty).
///
/// @param  selection  The selection handle.
///
ULExport void ulDOMSelectionRemoveAllRanges(ULDOMSelection selection);

///
/// Get a range with the selection's start and end (getRangeAt).
///
/// @param  selection  The selection handle.
///
/// @param  index      The index of the range (always 0, since a selection has at most one
///                    range).
///
/// @param  error      Receives the DOM error if one occurs (can be NULL).
///
/// @return  Returns a new range. You must call ulDestroyDOMRange() when finished. Returns NULL
///          and sets `error` to kULDOMErrorCode_IndexSizeError if `index` isn't less than the
///          range count.
///
/// @note  Each call returns a new copy. Changing the range doesn't change the selection.
///
ULExport ULDOMRange ulDOMSelectionGetRangeAt(ULDOMSelection selection, size_t index,
                                             ULDOMError* error);

///
/// Select a range (addRange).
///
/// If nothing is selected, the selection moves to the range. If something is selected (even just
/// a caret), a range that overlaps it grows the selection to cover both, and any other range is
/// ignored. To replace the selection, call ulDOMSelectionRemoveAllRanges() first.
///
/// @param  selection  The selection handle.
///
/// @param  range      The range to select.
///
/// @note  The selection takes a copy of the range's boundary points, so changing the range later
///        doesn't change the selection.
///
ULExport void ulDOMSelectionAddRange(ULDOMSelection selection, ULDOMRange range);

///
/// Remove the selected content from the page (deleteFromDocument).
///
/// The selection collapses to where the content was.
///
/// @param  selection  The selection handle.
///
ULExport void ulDOMSelectionDeleteFromDocument(ULDOMSelection selection);

///
/// Whether or not a node is in the selection (containsNode).
///
/// @param  selection  The selection handle.
///
/// @param  node       The node to check.
///
/// @param  partial    Pass true to count a node that's only partly selected.
///
/// @return  Returns true if the node is in the selection.
///
/// @note  A text node counts if any of its text is selected, even when `partial` is false.
///
ULExport bool ulDOMSelectionContainsNode(ULDOMSelection selection, ULDOMNode node,
                                         bool partial);

///
/// Move or extend the selection by a unit of text (modify), like the arrow keys do.
///
/// This is the non-standard Selection.modify() that browsers support.
///
/// @param  selection    The selection handle.
///
/// @param  alter        `move` to move the caret or `extend` to grow the selection.
///
/// @param  direction    `forward`, `backward`, `left`, or `right`.
///
/// @param  granularity  `character`, `word`, `sentence`, `line`, `paragraph`, `lineboundary`,
///                      `sentenceboundary`, `paragraphboundary`, or `documentboundary`.
///
/// @note  The values are null-terminated UTF-8 and ignore case. A call with any other value (or
///        NULL) does nothing.
///
ULExport void ulDOMSelectionModify(ULDOMSelection selection, const char* alter,
                                   const char* direction, const char* granularity);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMSELECTION_H
