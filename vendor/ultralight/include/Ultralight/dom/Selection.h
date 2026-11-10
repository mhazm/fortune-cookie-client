///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMSelection.h>
#include <Ultralight/dom/Document.h>
#include <Ultralight/dom/Error.h>
#include <Ultralight/dom/Node.h>
#include <Ultralight/dom/Range.h>
#include <Ultralight/dom/Window.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace ultralight {
namespace dom {

///
/// A handle to the active user selection or caret in a document.
///
/// A dom::Selection represents either a caret or highlighted text on a page. You obtain one by
/// calling Document::getSelection() or Window::getSelection().
///
/// The handle is live-- reading its properties reflects the user's current selection.
///
/// This example checks whether the user highlighted text and shows a quote button:
///
/// ```
/// dom::Selection selection = document.getSelection();
/// if (!selection.isCollapsed())
///   ShowQuoteButton(selection.toString());
/// ```
///
/// ## Anchor and Focus
///
/// A selection spans between two endpoints in the document tree:
///
/// - **The anchor marks where the selection starts.** Calling anchorNode() returns the containing
///   dom::Node, and anchorOffset() returns the offset within that node.
/// - **The focus marks where the selection ends.** Calling focusNode() and focusOffset() returns
///   its position, which can sit before the anchor when the user selects backward.
///
/// Each document has its own Selection (including each subframe).
///
/// ## Reacting to Selection Changes
///
/// Every change to the active selection dispatches a `selectionchange` event on the document. The
/// library delivers these events during a later Renderer::Update(), dispatching one event for each
/// change.
///
/// This example updates UI state whenever the selection changes:
///
/// ```
/// document.addEventListener("selectionchange", [selection] {
///   UpdateQuoteButton(selection.toString());
/// });
/// ```
///
/// ## Selecting a Range
///
/// To select a dom::Range, call removeAllRanges() before calling addRange(). On its own, addRange()
/// only expands an existing selection if the new range overlaps it, and it ignores any range that
/// doesn't overlap.
///
/// This example selects the contents of an element:
///
/// ```
/// dom::Range range = document.createRange();
/// range.selectNodeContents(document.getElementById("quote"));
/// selection.removeAllRanges();
/// selection.addRange(range);
/// ```
///
/// ## Selections in Text Fields
///
/// The selection doesn't enter editable form controls. When an `<input>` or `<textarea>` has focus,
/// anchorNode() and focusNode() sit just outside the element, and isCollapsed() returns true. To
/// read or change selected text inside an editable field, use the element's `selectionStart` and
/// `selectionEnd` properties instead (see dom::HTMLInputElement).
///
/// ## Differences from Web Browsers
///
/// Several methods handle node validation and range ownership differently from JavaScript:
///
/// - **Cross-document nodes are ignored.** Passing a node from another document (such as a
///   subframe) leaves the selection unchanged instead of throwing an exception.
/// - **Offsets past the end of a node fail only in extend().** Other placement methods like
///   collapse(), setBaseAndExtent(), and selectAllChildren() don't check whether an offset exceeds
///   the node's length.
/// - **getRangeAt() returns an independent copy.** Changes made to the returned dom::Range don't
///   alter the active selection.
///
/// After its page goes away, a Selection stops working like a dom::Element does (see that class).
///
/// @see  dom::Document::getSelection(), dom::Window::getSelection(), dom::Range,
///       dom::HTMLInputElement
///
class Selection {
 public:
  ///
  /// Create an empty Selection.
  ///
  Selection() {}

  ///
  /// Copy constructor (both handles refer to the same selection).
  ///
  /// @param  other  The Selection to copy.
  ///
  Selection(const Selection& other)
      : handle_(other.handle_ ? ulCreateDOMSelectionRef(other.handle_) : nullptr) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Selection to move from.
  ///
  Selection(Selection&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The Selection to assign from.
  ///
  /// @return  Returns this Selection.
  ///
  Selection& operator=(Selection other) noexcept {
    std::swap(handle_, other.handle_);
    return *this;
  }

  ///
  /// Destroy this handle (the selection itself isn't affected).
  ///
  ~Selection() { ulDestroyDOMSelection(handle_); }

  ///
  /// Whether or not this Selection is valid (see IsAlive()).
  ///
  explicit operator bool() const { return IsAlive(); }

  ///
  /// Whether or not this Selection is empty (it holds no handle).
  ///
  bool IsEmpty() const { return handle_ == nullptr; }

  ///
  /// Whether or not this Selection is valid (it isn't empty and its page is still alive).
  ///
  /// @note  Safe to call from any thread.
  ///
  bool IsAlive() const { return handle_ && ulDOMSelectionIsAlive(handle_); }

  // --- Reads ------------------------------------------------------------------------------

  ///
  /// Get the node the selection starts in (anchorNode).
  ///
  /// @return  Returns the node (empty if nothing is selected).
  ///
  Node anchorNode() const { return Node::Adopt(ulDOMSelectionGetAnchorNode(handle_)); }

  ///
  /// Get the offset of the anchor in anchorNode() (anchorOffset).
  ///
  size_t anchorOffset() const { return ulDOMSelectionGetAnchorOffset(handle_); }

  ///
  /// Get the node the selection ends in (focusNode).
  ///
  /// @return  Returns the node (empty if nothing is selected).
  ///
  Node focusNode() const { return Node::Adopt(ulDOMSelectionGetFocusNode(handle_)); }

  ///
  /// Get the offset of the focus in focusNode() (focusOffset).
  ///
  size_t focusOffset() const { return ulDOMSelectionGetFocusOffset(handle_); }

  ///
  /// Whether or not the selection is a caret (isCollapsed).
  ///
  /// @return  Returns true for a caret, and also when nothing is selected (or the Selection is
  ///          empty or its page is gone).
  ///
  bool isCollapsed() const { return ulDOMSelectionIsCollapsed(handle_); }

  ///
  /// Get the number of ranges in the selection (rangeCount).
  ///
  /// @return  Returns 0 if nothing is selected, otherwise 1.
  ///
  size_t rangeCount() const { return ulDOMSelectionGetRangeCount(handle_); }

  ///
  /// Get the kind of selection (type).
  ///
  /// @return  Returns `None` (nothing is selected), `Caret`, or `Range`.
  ///
  std::string type() const { return detail::TakeString(ulDOMSelectionGetType(handle_)); }

  ///
  /// Get the selected text (toString).
  ///
  std::string toString() const {
    return detail::TakeString(ulDOMSelectionToString(handle_));
  }

  // --- Placing the selection --------------------------------------------------------------

  ///
  /// Place a caret (collapse or setPosition).
  ///
  /// @param  node    The node to put the caret in. Pass an empty Node to clear the selection.
  ///
  /// @param  offset  The offset in `node` (like a Range boundary point).
  ///
  /// @note  Unlike the web, an `offset` past the end of `node` doesn't fail, so check it yourself.
  ///
  void collapse(const Node& node, size_t offset = 0) const {
    ulDOMSelectionCollapse(handle_, node.raw(), offset, nullptr);
  }

  ///
  /// Same as collapse(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this Selection is empty, or its page or
  ///          `node`'s page is gone).
  ///
  [[nodiscard]] Result<void> collapse(const Node& node, Checked_t) const {
    return collapse(node, 0, Checked);
  }

  ///
  /// Same as collapse() with an `offset`, but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this Selection is empty, or its page or
  ///          `node`'s page is gone).
  ///
  [[nodiscard]] Result<void> collapse(const Node& node, size_t offset, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMSelectionCollapse(handle_, node.raw(), offset, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Collapse the selection to its start (collapseToStart). Does nothing if nothing is selected.
  ///
  void collapseToStart() const { ulDOMSelectionCollapseToStart(handle_, nullptr); }

  ///
  /// Same as collapseToStart(), but returns a Result with the reason for a failure (eg, an
  /// InvalidStateError if nothing is selected).
  ///
  /// @return  Returns success. Fails with an InvalidStateError if nothing is selected.
  ///
  [[nodiscard]] Result<void> collapseToStart(Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMSelectionCollapseToStart(handle_, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Collapse the selection to its end (collapseToEnd). Does nothing if nothing is selected.
  ///
  void collapseToEnd() const { ulDOMSelectionCollapseToEnd(handle_, nullptr); }

  ///
  /// Same as collapseToEnd(), but returns a Result with the reason for a failure (eg, an
  /// InvalidStateError if nothing is selected).
  ///
  /// @return  Returns success. Fails with an InvalidStateError if nothing is selected.
  ///
  [[nodiscard]] Result<void> collapseToEnd(Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMSelectionCollapseToEnd(handle_, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Move the focus and keep the anchor where it is (extend).
  ///
  /// Use this to grow or shrink the selection from one end.
  ///
  /// @param  node    The node to move the focus into.
  ///
  /// @param  offset  The offset in `node` (like a Range boundary point).
  ///
  /// @note  Nothing changes if nothing is selected or `offset` is past the end of `node`.
  ///
  void extend(const Node& node, size_t offset = 0) const {
    ulDOMSelectionExtend(handle_, node.raw(), offset, nullptr);
  }

  ///
  /// Same as extend(), but returns a Result with the reason for a failure (eg, an
  /// InvalidStateError if nothing is selected).
  ///
  /// @return  Returns success. Fails with an InvalidStateError if nothing is selected, or an
  ///          IndexSizeError if `offset` is past the end of `node`.
  ///
  [[nodiscard]] Result<void> extend(const Node& node, Checked_t) const {
    return extend(node, 0, Checked);
  }

  ///
  /// Same as extend() with an `offset`, but returns a Result with the reason for a failure (eg,
  /// an IndexSizeError if `offset` is past the end of `node`).
  ///
  /// @return  Returns success. Fails with an InvalidStateError if nothing is selected, or an
  ///          IndexSizeError if `offset` is past the end of `node`.
  ///
  [[nodiscard]] Result<void> extend(const Node& node, size_t offset, Checked_t) const {
    if (IsEmpty() || node.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMSelectionExtend(handle_, node.raw(), offset, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Set the anchor and the focus in one call (setBaseAndExtent).
  ///
  /// The focus can come before the anchor in the document.
  ///
  /// @param  anchor_node    The node for the anchor.
  ///
  /// @param  anchor_offset  The offset in `anchor_node`.
  ///
  /// @param  focus_node     The node for the focus.
  ///
  /// @param  focus_offset   The offset in `focus_node`.
  ///
  /// \parblock
  /// @note  If one node is empty, the selection becomes a caret at the other point. If both are
  ///        empty, the selection is cleared.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Unlike the web, an offset past the end of its node doesn't fail, so check offsets
  ///        yourself.
  /// \endparblock
  ///
  void setBaseAndExtent(const Node& anchor_node, size_t anchor_offset, const Node& focus_node,
                        size_t focus_offset) const {
    ulDOMSelectionSetBaseAndExtent(handle_, anchor_node.raw(), anchor_offset, focus_node.raw(),
                                   focus_offset, nullptr);
  }

  ///
  /// Same as setBaseAndExtent(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this Selection is empty, or its page or a node's
  ///          page is gone).
  ///
  [[nodiscard]] Result<void> setBaseAndExtent(const Node& anchor_node,
                                              size_t anchor_offset,
                                              const Node& focus_node,
                                              size_t focus_offset,
                                              Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMSelectionSetBaseAndExtent(handle_, anchor_node.raw(), anchor_offset,
                                       focus_node.raw(), focus_offset, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Select everything inside a node (selectAllChildren).
  ///
  /// @param  node  The node whose children to select. A text node has no children, so this places
  ///               a caret at its start.
  ///
  void selectAllChildren(const Node& node) const {
    ulDOMSelectionSelectAllChildren(handle_, node.raw(), nullptr);
  }

  ///
  /// Same as selectAllChildren(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when a handle is empty or its page is gone).
  ///
  [[nodiscard]] Result<void> selectAllChildren(const Node& node, Checked_t) const {
    if (IsEmpty() || node.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMSelectionSelectAllChildren(handle_, node.raw(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Clear the selection (removeAllRanges or empty).
  ///
  void removeAllRanges() const { ulDOMSelectionRemoveAllRanges(handle_); }

  ///
  /// Select a range (addRange).
  ///
  /// If nothing is selected, the selection moves to the range. If something is selected (even just
  /// a caret), a range that overlaps it grows the selection to cover both, and any other range is
  /// ignored. To replace the selection, call removeAllRanges() first.
  ///
  /// @param  range  The range to select.
  ///
  /// @note  The selection takes a copy of the range's boundary points, so changing the range
  ///        later doesn't change the selection.
  ///
  void addRange(const Range& range) const { ulDOMSelectionAddRange(handle_, range.raw()); }

  // --- Working with the selection ---------------------------------------------------------

  ///
  /// Get a range with the selection's start and end (getRangeAt).
  ///
  /// @param  index  The index of the range (always 0, since a selection has at most one range).
  ///
  /// @return  Returns a new Range (empty if `index` isn't less than rangeCount()).
  ///
  /// @note  Each call returns a new copy. Changing the Range doesn't change the selection.
  ///
  Range getRangeAt(size_t index) const {
    return Range::Adopt(ulDOMSelectionGetRangeAt(handle_, index, nullptr));
  }

  ///
  /// Same as getRangeAt(), but returns a Result with the reason for a failure (eg, an
  /// IndexSizeError if `index` isn't less than rangeCount()).
  ///
  /// @return  Returns a new Range. Fails with an IndexSizeError if `index` isn't less than
  ///          rangeCount().
  ///
  [[nodiscard]] Result<Range> getRangeAt(size_t index, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ULDOMRange range = ulDOMSelectionGetRangeAt(handle_, index, error.out()))
      return Range::Adopt(range);
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Remove the selected content from the page (deleteFromDocument).
  ///
  /// The selection collapses to where the content was.
  ///
  void deleteFromDocument() const { ulDOMSelectionDeleteFromDocument(handle_); }

  ///
  /// Whether or not a node is in the selection (containsNode).
  ///
  /// @param  node     The node to check.
  ///
  /// @param  partial  Pass true to count a node that's only partly selected.
  ///
  /// @return  Returns true if the node is in the selection.
  ///
  /// @note  A text node counts if any of its text is selected, even when `partial` is false.
  ///
  bool containsNode(const Node& node, bool partial = false) const {
    return ulDOMSelectionContainsNode(handle_, node.raw(), partial);
  }

  ///
  /// Move or extend the selection by a unit of text (modify), like the arrow keys do.
  ///
  /// This is the non-standard Selection.modify() that browsers support.
  ///
  /// @param  alter        `move` to move the caret or `extend` to grow the selection.
  ///
  /// @param  direction    `forward`, `backward`, `left`, or `right`.
  ///
  /// @param  granularity  `character`, `word`, `sentence`, `line`, `paragraph`, `lineboundary`,
  ///                      `sentenceboundary`, `paragraphboundary`, or `documentboundary`.
  ///
  /// @note  The values ignore case. A call with any other value does nothing.
  ///
  void modify(std::string_view alter, std::string_view direction,
              std::string_view granularity) const {
    detail::CString a(alter);
    detail::CString d(direction);
    detail::CString g(granularity);
    ulDOMSelectionModify(handle_, a.c_str(), d.c_str(), g.c_str());
  }

  // --- Interop with the C API (most embedders never touch raw handles) -----------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMSelection() (NULL gives an empty Selection).
  ///
  /// @return  Returns a Selection that destroys `handle` when it's done.
  ///
  static Selection Adopt(ULDOMSelection handle) { return Selection(handle); }

  ///
  /// Wrap a C handle you don't own, adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Selection).
  ///
  /// @return  Returns a Selection with its own reference, so you can keep it as long as you like.
  ///
  static Selection FromBorrowed(ULDOMSelection handle) {
    return Selection(handle ? ulCreateDOMSelectionRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMSelection.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Selection). This Selection still owns it, so
  ///          don't destroy it.
  ///
  ULDOMSelection raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This Selection becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMSelection() when finished.
  ///
  ULDOMSelection LeakRef() {
    ULDOMSelection handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit Selection(ULDOMSelection handle) : handle_(handle) {}

  ULDOMSelection handle_ = nullptr;
};

inline Selection Document::getSelection() const {
  return Selection::Adopt(ulDOMDocumentGetSelection(handle_));
}

inline Selection Window::getSelection() const {
  ULDOMDocument doc = ulDOMWindowGetDocument(handle_);
  Selection selection = Selection::Adopt(ulDOMDocumentGetSelection(doc));
  ulDestroyDOMDocument(doc);
  return selection;
}

}  // namespace dom
}  // namespace ultralight
