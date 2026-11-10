///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMDocument.h>
#include <Ultralight/dom/Element.h>

#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {

///
/// A container for assembling DOM nodes off the page.
///
/// A DocumentFragment is a DOM node that holds child nodes outside the document tree. It serves as
/// temporary storage for building content in native code before inserting it into the page.
///
/// You can build list items from C++ data off the page and insert them in one call:
///
/// ```
/// dom::DocumentFragment items = document.createDocumentFragment();
/// for (const Score& score : scores) {
///   dom::Element entry = document.createElement("li");
///   entry.append(score.player, ": ", std::to_string(score.points));
///   items.appendChild(entry);
/// }
/// document.getElementById("scores").replaceChildren(items);
/// ```
///
/// ## Inserting a Fragment
///
/// Because DocumentFragment inherits from dom::Node, you can pass it to any DOM insertion call.
///
/// Inserting a fragment moves all its children into the page in order, leaving the fragment empty.
/// The handle remains valid after insertion, so you can fill it with new nodes and insert it again.
///
/// @see  dom::Document::createDocumentFragment(), dom::Element::replaceChildren(), dom::Node,
///       dom::Range::extractContents()
///
class DocumentFragment : public Node {
 public:
  ///
  /// Create an empty DocumentFragment.
  ///
  /// @see  Document::createDocumentFragment()
  ///
  DocumentFragment() = default;

  ///
  /// Copy constructor (both handles refer to the same fragment).
  ///
  /// @param  other  The DocumentFragment to copy.
  ///
  DocumentFragment(const DocumentFragment& other) : Node(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The DocumentFragment to move from.
  ///
  DocumentFragment(DocumentFragment&& other) noexcept : Node(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The DocumentFragment to assign from.
  ///
  /// @return  Returns this DocumentFragment.
  ///
  DocumentFragment& operator=(DocumentFragment other) noexcept {
    std::swap(detail_.handle, other.detail_.handle);
    return *this;
  }

  // Validity (operator bool, IsEmpty, IsAlive), IsSame(), destruction, and the node-level
  // surface (childNodes(), firstChild(), hasChildNodes(), textContent, ...) are inherited from
  // Node: a DocumentFragment IS a Node, and the handle families share one object underneath.

  ///
  /// Append an element to the fragment (appendChild).
  ///
  /// If the element is already in the page (or in another fragment), it moves here.
  ///
  /// @param  child  The element to append.
  ///
  /// @return  Returns the appended element (empty if this fragment or `child` is empty or its page
  ///          is gone).
  ///
  Element appendChild(const Element& child) const {
    return ulDOMFragmentAppendElement(raw(), child.raw(), nullptr) ? child : Element();
  }

  ///
  /// Same as appendChild(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns the appended element (it fails only when this fragment or `child` is empty
  ///          or its page is gone).
  ///
  [[nodiscard]] Result<Element> appendChild(const Element& child, Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMFragmentAppendElement(raw(), child.raw(), error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Append a node of any kind to the fragment (appendChild).
  ///
  /// Use this for text and comment nodes (eg, from Document::createTextNode()). An Element
  /// argument goes to the overload that returns an Element.
  ///
  /// @param  child  The node to append. If it's already in the page (or in another fragment), it
  ///                moves here.
  ///
  /// @return  Returns `child` (empty if nothing was appended, eg, because `child` is a doctype, or
  ///          a handle is empty or its page is gone).
  ///
  Node appendChild(const Node& child) const {
    ULDOMNodeOrString item = detail::NodeItem(child);
    return ulDOMFragmentAppend(raw(), &item, 1, nullptr) ? child : Node();
  }

  ///
  /// Same as appendChild() with a Node, but returns a Result with the reason for a failure.
  ///
  /// @return  Returns `child`. Fails with a HierarchyRequestError if `child` can't go in a
  ///          fragment (eg, a doctype).
  ///
  [[nodiscard]] Result<Node> appendChild(const Node& child, Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    ULDOMNodeOrString item = detail::NodeItem(child);
    detail::ErrorScope error;
    if (ulDOMFragmentAppend(raw(), &item, 1, error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Add nodes and strings to the end of the fragment (append).
  ///
  /// Pass any mix of nodes (including elements) and strings. Each string is added as a new text
  /// node (it's never parsed as markup).
  ///
  /// @param  nodes  The nodes and strings to add, in order. A node that's already in the page (or
  ///                in another fragment) moves here.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void append(T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    ulDOMFragmentAppend(raw(), items.data(), items.size(), nullptr);
  }

  ///
  /// Same as append(), but returns a Result with the reason for a failure.
  ///
  /// dom::Checked comes first here, before the nodes (`batch.append(dom::Checked, row, "x")`).
  ///
  /// @return  Returns success. Fails with a HierarchyRequestError if one of the nodes can't go in
  ///          a fragment (eg, a doctype).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> append(Checked_t, T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMFragmentAppend(raw(), items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Add nodes and strings to the start of the fragment, before its first child (prepend).
  ///
  /// Works like append().
  ///
  /// @param  nodes  The nodes and strings to add, in order. A node that's already in the page (or
  ///                in another fragment) moves here.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void prepend(T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    ulDOMFragmentPrepend(raw(), items.data(), items.size(), nullptr);
  }

  ///
  /// Same as prepend(), but returns a Result with the reason for a failure.
  ///
  /// dom::Checked comes first here, before the nodes (`batch.prepend(dom::Checked, row, "x")`).
  ///
  /// @return  Returns success. Fails with a HierarchyRequestError if one of the nodes can't go in
  ///          a fragment (eg, a doctype).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> prepend(Checked_t, T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMFragmentPrepend(raw(), items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------
  //
  // These deliberately hide Node's same-named members with fragment-typed versions, as
  // Element's do: ULDOMNode and ULDOMFragment name one object underneath, so the casts below
  // are exact by construction.

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMFragment() (NULL gives an empty DocumentFragment).
  ///
  /// @return  Returns a DocumentFragment that destroys `handle` when it's done.
  ///
  static DocumentFragment Adopt(ULDOMFragment handle) { return DocumentFragment(handle); }

  ///
  /// Wrap a C handle the library owns, adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty DocumentFragment).
  ///
  /// @return  Returns a DocumentFragment with its own reference to `handle`.
  ///
  static DocumentFragment FromBorrowed(ULDOMFragment handle) {
    return DocumentFragment(handle ? ulCreateDOMFragmentRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the C API (eg, ulDOMElementAppendFragment()).
  ///
  /// @return  Returns the handle (NULL for an empty DocumentFragment). This DocumentFragment still
  ///          owns it, so don't destroy it.
  ///
  ULDOMFragment raw() const { return reinterpret_cast<ULDOMFragment>(detail_.handle); }

  ///
  /// Give up ownership of the C handle and return it. This DocumentFragment becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMFragment() when finished.
  ///
  ULDOMFragment LeakRef() { return reinterpret_cast<ULDOMFragment>(Node::LeakRef()); }

 protected:
  explicit DocumentFragment(ULDOMFragment handle) : Node(reinterpret_cast<ULDOMNode>(handle)) {}
};

// DocumentFragment adds no data to Node: its copy, move, and assignment transfer only the Node
// handle, and a DocumentFragment passed or sliced to a Node loses nothing.
static_assert(std::is_standard_layout_v<DocumentFragment>,
              "DocumentFragment must stay standard-layout");
static_assert(sizeof(DocumentFragment) == sizeof(Node),
              "DocumentFragment must not declare data members of its own");

inline void Element::appendChild(const DocumentFragment& fragment) const {
  ulDOMElementAppendFragment(raw(), fragment.raw(), nullptr);
}

inline Result<void> Element::appendChild(const DocumentFragment& fragment, Checked_t) const {
  if (IsEmpty() || fragment.IsEmpty())
    return detail::EmptyHandleError();
  detail::ErrorScope error;
  if (ulDOMElementAppendFragment(raw(), fragment.raw(), error.out()))
    return {};
  return Unexpected<Error>(error.TakeError());
}

inline DocumentFragment Node::AsFragment() const {
  return DocumentFragment::Adopt(ulDOMNodeAsFragment(detail_.handle));
}

}  // namespace dom
}  // namespace ultralight
