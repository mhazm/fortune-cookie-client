///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMRange.h>
#include <Ultralight/dom/DOMRect.h>
#include <Ultralight/dom/DocumentFragment.h>
#include <Ultralight/dom/Error.h>
#include <Ultralight/dom/Node.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ultralight {
namespace dom {

///
/// A handle to a live range (a span of a page between two boundary points).
///
/// A Range works like the JavaScript %Range. Get one from Document::createRange() and point it at
/// some content:
///
/// ```
/// dom::Range range = document.createRange();
/// range.selectNodeContents(para);
/// std::string text = range.toString();
/// dom::DOMRect box = range.getBoundingClientRect();
/// ```
///
/// Copying a Range copies the handle, so both refer to the same range. Use cloneRange() to make
/// an independent copy.
///
/// After its page goes away, a Range stops working like a dom::Element does (see that class).
///
/// ## Boundary Points
///
/// Each boundary point is a node plus an offset. In a text or comment node the offset counts
/// UTF-16 code units. In any other node it counts children.
///
///  - **A new range starts at the document.** A boundary point in the document itself reads as an
///    empty Node (the document is never a Node), so point the range at real content first.
///  - **Boundary nodes must come from the range's own document.** A node from another document
///    (eg, a subframe's) leaves the range unchanged (with dom::Checked, the call fails with a
///    WrongDocumentError).
///
/// ## Live Updates
///
/// The range stays valid while the page changes. When nodes are inserted or removed, the library
/// moves the boundary points to match. If a boundary point's node is removed, the point moves to
/// where that node was in its parent.
///
/// Each live range adds a little work to every DOM change in its document, so destroy a Range once
/// you're done with it.
///
class Range {
 public:
  ///
  /// Which boundary points compareBoundaryPoints() compares (the web's Range.START_TO_START
  /// constants).
  ///
  enum class CompareHow : uint8_t {
    StartToStart = kULDOMRangeCompareHow_StartToStart,  ///< Compare the two start points.
    StartToEnd = kULDOMRangeCompareHow_StartToEnd,      ///< This end vs the other's start.
    EndToEnd = kULDOMRangeCompareHow_EndToEnd,          ///< Compare the two end points.
    EndToStart = kULDOMRangeCompareHow_EndToStart,      ///< This start vs the other's end.
  };

  ///
  /// Create an empty Range.
  ///
  Range() {}

  ///
  /// Copy constructor (both handles refer to the same range).
  ///
  /// @param  other  The Range to copy.
  ///
  Range(const Range& other)
      : handle_(other.handle_ ? ulCreateDOMRangeRef(other.handle_) : nullptr) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Range to move from.
  ///
  Range(Range&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The Range to assign from.
  ///
  /// @return  Returns this Range.
  ///
  Range& operator=(Range other) noexcept {
    std::swap(handle_, other.handle_);
    return *this;
  }

  ///
  /// Destroy this handle (the page isn't affected).
  ///
  ~Range() { ulDestroyDOMRange(handle_); }

  ///
  /// Whether or not this Range is valid (see IsAlive()).
  ///
  explicit operator bool() const { return IsAlive(); }

  ///
  /// Whether or not this Range is empty (it holds no handle).
  ///
  bool IsEmpty() const { return handle_ == nullptr; }

  ///
  /// Whether or not this Range is valid (it isn't empty and its page is still alive).
  ///
  /// @note  Safe to call from any thread.
  ///
  bool IsAlive() const { return handle_ && ulDOMRangeIsAlive(handle_); }

  // --- Boundary reads ---------------------------------------------------------------------

  ///
  /// Get the node that holds the range's start (startContainer).
  ///
  /// @return  Returns the node (empty if the start is in the document itself).
  ///
  Node startContainer() const {
    return Node::Adopt(ulDOMRangeGetStartContainer(handle_));
  }

  ///
  /// Get the offset of the range's start in startContainer() (startOffset).
  ///
  size_t startOffset() const { return ulDOMRangeGetStartOffset(handle_); }

  ///
  /// Get the node that holds the range's end (endContainer).
  ///
  /// @return  Returns the node (empty if the end is in the document itself).
  ///
  Node endContainer() const { return Node::Adopt(ulDOMRangeGetEndContainer(handle_)); }

  ///
  /// Get the offset of the range's end in endContainer() (endOffset).
  ///
  size_t endOffset() const { return ulDOMRangeGetEndOffset(handle_); }

  ///
  /// Whether or not the range's start and end are the same point (collapsed).
  ///
  /// @return  Returns true if the range is collapsed (also for an empty Range or one whose page
  ///          is gone).
  ///
  bool collapsed() const { return ulDOMRangeGetCollapsed(handle_); }

  ///
  /// Get the deepest node that contains both boundary points (commonAncestorContainer).
  ///
  /// @return  Returns the node (empty if it's the document itself).
  ///
  Node commonAncestorContainer() const {
    return Node::Adopt(ulDOMRangeGetCommonAncestorContainer(handle_));
  }

  // --- Boundary writes --------------------------------------------------------------------

  ///
  /// Set the range's start (setStart).
  ///
  /// If the new start is after the range's end, the range collapses to the new start.
  ///
  /// @param  node    The node for the new start (from this range's document).
  ///
  /// @param  offset  The offset in `node` (see Boundary Points above).
  ///
  /// @note  Nothing changes if `offset` is past the end of `node`, `node` is a doctype, or `node`
  ///        is from another document.
  ///
  void setStart(const Node& node, size_t offset) const {
    ulDOMRangeSetStart(handle_, node.raw(), offset, nullptr);
  }

  ///
  /// Same as setStart(), but returns a Result with the reason for a failure (eg, an IndexSizeError
  /// if `offset` is past the end of `node`).
  ///
  /// @return  Returns success. Fails with an IndexSizeError if `offset` is past the end of `node`,
  ///          an InvalidNodeTypeError if `node` is a doctype, or a WrongDocumentError if `node` is
  ///          from another document.
  ///
  [[nodiscard]] Result<void> setStart(const Node& node, size_t offset, Checked_t) const {
    if (IsEmpty() || node.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMRangeSetStart(handle_, node.raw(), offset, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Set the range's end (setEnd).
  ///
  /// If the new end is before the range's start, the range collapses to the new end.
  ///
  /// @param  node    The node for the new end (from this range's document).
  ///
  /// @param  offset  The offset in `node` (see Boundary Points above).
  ///
  /// @note  Nothing changes if `offset` is past the end of `node`, `node` is a doctype, or `node`
  ///        is from another document.
  ///
  void setEnd(const Node& node, size_t offset) const {
    ulDOMRangeSetEnd(handle_, node.raw(), offset, nullptr);
  }

  ///
  /// Same as setEnd(), but returns a Result with the reason for a failure (eg, an IndexSizeError
  /// if `offset` is past the end of `node`).
  ///
  /// @return  Returns success. Fails with an IndexSizeError if `offset` is past the end of `node`,
  ///          an InvalidNodeTypeError if `node` is a doctype, or a WrongDocumentError if `node` is
  ///          from another document.
  ///
  [[nodiscard]] Result<void> setEnd(const Node& node, size_t offset, Checked_t) const {
    if (IsEmpty() || node.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMRangeSetEnd(handle_, node.raw(), offset, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Collapse the range to its start or its end (collapse).
  ///
  /// @param  to_start  Pass true to collapse to the start and false to collapse to the end.
  ///
  void collapse(bool to_start = false) const { ulDOMRangeCollapse(handle_, to_start); }

  ///
  /// Make the range cover a whole node (selectNode).
  ///
  /// The range starts just before `node` and ends just after it, in `node`'s parent.
  ///
  /// @param  node  The node to cover (from this range's document).
  ///
  /// @note  Nothing changes if `node` has no parent or is from another document.
  ///
  void selectNode(const Node& node) const { ulDOMRangeSelectNode(handle_, node.raw(), nullptr); }

  ///
  /// Same as selectNode(), but returns a Result with the reason for a failure (eg, an
  /// InvalidNodeTypeError if `node` has no parent).
  ///
  /// @return  Returns success. Fails with an InvalidNodeTypeError if `node` has no parent, or a
  ///          WrongDocumentError if `node` is from another document.
  ///
  [[nodiscard]] Result<void> selectNode(const Node& node, Checked_t) const {
    if (IsEmpty() || node.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMRangeSelectNode(handle_, node.raw(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Make the range cover everything inside a node (selectNodeContents).
  ///
  /// For an element that's all of its children. For a text or comment node it's all of its text.
  ///
  /// @param  node  The node whose contents to cover (from this range's document).
  ///
  /// @note  Nothing changes if `node` is a doctype or is from another document.
  ///
  void selectNodeContents(const Node& node) const {
    ulDOMRangeSelectNodeContents(handle_, node.raw(), nullptr);
  }

  ///
  /// Same as selectNodeContents(), but returns a Result with the reason for a failure (eg, a
  /// WrongDocumentError if `node` is from another document).
  ///
  /// @return  Returns success. Fails with an InvalidNodeTypeError if `node` is a doctype, or a
  ///          WrongDocumentError if `node` is from another document.
  ///
  [[nodiscard]] Result<void> selectNodeContents(const Node& node, Checked_t) const {
    if (IsEmpty() || node.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMRangeSelectNodeContents(handle_, node.raw(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  // --- Comparison -------------------------------------------------------------------------

  ///
  /// Compare a boundary point of this range with one of another range (compareBoundaryPoints).
  ///
  /// @param  how    Which point of each range to compare (see CompareHow).
  ///
  /// @param  other  The range to compare with.
  ///
  /// @return  Returns -1, 0, or 1 if this range's point is before, at, or after the other range's
  ///          point. Also returns 0 if the call fails (eg, the ranges are in different documents),
  ///          so use the dom::Checked overload to tell a failure from equal points.
  ///
  int compareBoundaryPoints(CompareHow how, const Range& other) const {
    int result = 0;
    ulDOMRangeCompareBoundaryPoints(handle_, static_cast<ULDOMRangeCompareHow>(how),
                                    other.handle_, &result, nullptr);
    return result;
  }

  ///
  /// Same as compareBoundaryPoints(), but returns a Result with the reason for a failure (eg, a
  /// WrongDocumentError if the ranges are in different documents).
  ///
  /// @return  Returns -1, 0, or 1 if this range's point is before, at, or after the other range's
  ///          point. Fails with a WrongDocumentError if the two points aren't in the same tree
  ///          (eg, the ranges are in different documents).
  ///
  [[nodiscard]] Result<int> compareBoundaryPoints(CompareHow how, const Range& other,
                                                  Checked_t) const {
    if (IsEmpty() || other.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    int result = 0;
    if (ulDOMRangeCompareBoundaryPoints(handle_,
                                        static_cast<ULDOMRangeCompareHow>(how),
                                        other.handle_, &result, error.out()))
      return result;
    return Unexpected<Error>(error.TakeError());
  }

  // --- Contents ---------------------------------------------------------------------------

  ///
  /// Remove the range's contents from the page (deleteContents).
  ///
  /// Text at the edges of the range is trimmed, and the range collapses to where the contents were.
  ///
  void deleteContents() const { ulDOMRangeDeleteContents(handle_, nullptr); }

  ///
  /// Same as deleteContents(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this range is empty or its page is gone).
  ///
  [[nodiscard]] Result<void> deleteContents(Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMRangeDeleteContents(handle_, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Move the range's contents out of the page into a new fragment (extractContents).
  ///
  /// A node that's only partly inside the range stays in the page without the covered part, and
  /// the fragment gets a copy that holds that part. The range collapses to where the contents were.
  ///
  /// @return  Returns the fragment (empty if the range contains a doctype or its page is gone, in
  ///          which case nothing is removed).
  ///
  DocumentFragment extractContents() const {
    return DocumentFragment::Adopt(ulDOMRangeExtractContents(handle_, nullptr));
  }

  ///
  /// Same as extractContents(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError if the range contains a doctype).
  ///
  /// @return  Returns the fragment. Fails with a HierarchyRequestError if the range contains a
  ///          doctype.
  ///
  [[nodiscard]] Result<DocumentFragment> extractContents(Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ULDOMFragment fragment = ulDOMRangeExtractContents(handle_, error.out()))
      return DocumentFragment::Adopt(fragment);
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Copy the range's contents into a new fragment (cloneContents). The page doesn't change.
  ///
  /// @return  Returns the fragment (empty if the range contains a doctype or its page is gone).
  ///
  DocumentFragment cloneContents() const {
    return DocumentFragment::Adopt(ulDOMRangeCloneContents(handle_, nullptr));
  }

  ///
  /// Same as cloneContents(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError if the range contains a doctype).
  ///
  /// @return  Returns the fragment. Fails with a HierarchyRequestError if the range contains a
  ///          doctype.
  ///
  [[nodiscard]] Result<DocumentFragment> cloneContents(Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ULDOMFragment fragment = ulDOMRangeCloneContents(handle_, error.out()))
      return DocumentFragment::Adopt(fragment);
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Insert a node at the range's start (insertNode).
  ///
  /// If the start is inside a text node, the text node is split there. If the range is collapsed,
  /// it grows to cover the inserted node.
  ///
  /// @param  node  The node to insert (from this range's document). If it already has a parent,
  ///               it's moved.
  ///
  /// @note  Nothing is inserted if `node` can't go there (eg, the start is inside a comment, or
  ///        `node` contains the start) or `node` is from another document.
  ///
  void insertNode(const Node& node) const { ulDOMRangeInsertNode(handle_, node.raw(), nullptr); }

  ///
  /// Same as insertNode(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError if `node` can't go there).
  ///
  /// @return  Returns success. Fails with a HierarchyRequestError if `node` can't go there (eg,
  ///          the start is inside a comment, or `node` contains the start), or a
  ///          WrongDocumentError if `node` is from another document.
  ///
  [[nodiscard]] Result<void> insertNode(const Node& node, Checked_t) const {
    if (IsEmpty() || node.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMRangeInsertNode(handle_, node.raw(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Create an independent copy of the range (cloneRange).
  ///
  /// @return  Returns a new Range with the same boundary points. From then on, each range updates
  ///          on its own.
  ///
  Range cloneRange() const { return Range(ulDOMRangeCloneRange(handle_)); }

  ///
  /// Get the text the range covers (toString).
  ///
  /// @return  Returns the text of the text nodes in the range (comments aren't included).
  ///
  std::string toString() const {
    return detail::TakeString(ulDOMRangeToString(handle_));
  }

  // --- Geometry ---------------------------------------------------------------------------

  ///
  /// Get the smallest rectangle that contains everything the range covers (getBoundingClientRect).
  ///
  /// @return  Returns the rectangle in the same viewport coordinates as
  ///          Element::getBoundingClientRect() (all zero if nothing in the range is displayed).
  ///
  /// @note  This reads the layout, so it forces a synchronous style and layout pass if the document
  ///        has pending changes.
  ///
  DOMRect getBoundingClientRect() const {
    ULDOMRect rect = ulDOMRangeGetBoundingClientRect(handle_);
    return DOMRect { rect.x, rect.y, rect.width, rect.height };
  }

  ///
  /// Get a rectangle for each box the range covers (getClientRects).
  ///
  /// Text that wraps gives one rectangle per line. An element that's fully inside the range adds
  /// its own border box too.
  ///
  /// @return  Returns the rectangles in document order, in the same viewport coordinates as
  ///          Element::getBoundingClientRect().
  ///
  /// @note  This reads the layout, so it forces a synchronous style and layout pass if the document
  ///        has pending changes.
  ///
  std::vector<DOMRect> getClientRects() const {
    // Each waist call walks the range's boxes after a layout update, so query into a
    // local buffer first and pay a second walk only when a range spans more lines than it
    // holds (rare: one rectangle per line is the common shape).
    ULDOMRect local[8];
    size_t count = ulDOMRangeGetClientRects(handle_, local, 8);
    std::vector<DOMRect> rects;
    rects.reserve(count);
    if (count <= 8) {
      for (size_t i = 0; i < count; i++)
        rects.push_back(DOMRect { local[i].x, local[i].y, local[i].width, local[i].height });
      return rects;
    }
    std::vector<ULDOMRect> raw_rects(count);
    ulDOMRangeGetClientRects(handle_, raw_rects.data(), raw_rects.size());
    for (const ULDOMRect& rect : raw_rects)
      rects.push_back(DOMRect { rect.x, rect.y, rect.width, rect.height });
    return rects;
  }

  // --- Interop with the C API (most embedders never touch raw handles) -----------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMRange() (NULL gives an empty Range).
  ///
  /// @return  Returns a Range that destroys `handle` when it's done.
  ///
  static Range Adopt(ULDOMRange handle) { return Range(handle); }

  ///
  /// Wrap a C handle you don't own, adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Range).
  ///
  /// @return  Returns a Range with its own reference, so you can keep it as long as you like.
  ///
  static Range FromBorrowed(ULDOMRange handle) {
    return Range(handle ? ulCreateDOMRangeRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMRange.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Range). This Range still owns it, so don't
  ///          destroy it.
  ///
  ULDOMRange raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This Range becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMRange() when finished.
  ///
  ULDOMRange LeakRef() {
    ULDOMRange handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit Range(ULDOMRange handle) : handle_(handle) {}

  ULDOMRange handle_ = nullptr;
};

}  // namespace dom
}  // namespace ultralight
