///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_DOMNode.h>
#include <Ultralight/dom/Error.h>
#include <Ultralight/dom/detail/Properties.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {

class Document;
class DocumentFragment;
class Element;
class Node;
class NodeList;

/// \cond INTERNAL
namespace detail {

// An argument the variadic insertion methods (append(), before(), ...) accept: a handle to any
// node (an Element or a DocumentFragment too), or text as anything that converts to
// std::string_view.
template <typename T>
concept NodeOrString = std::is_convertible_v<T&&, const Node&>
    || (std::is_convertible_v<T&&, std::string_view>
        && !std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>);

inline ULDOMNodeOrString NodeItem(const Node& node);

template <typename T>
ULDOMNodeOrString NodeOrStringItem(T&& item) {
  if constexpr (std::is_convertible_v<T&&, const Node&>) {
    return NodeItem(item);
  } else {
    std::string_view text = item;
    return { nullptr, text.data() ? text.data() : "", text.size() };
  }
}

// The C items for an argument pack. They borrow from the arguments, which outlive the call they
// are passed to (temporaries last until the end of the caller's full-expression).
template <typename... T>
std::array<ULDOMNodeOrString, sizeof...(T)> NodeOrStringItems(T&&... items) {
  return { NodeOrStringItem(std::forward<T>(items))... };
}

}  // namespace detail
/// \endcond

///
/// The kinds of DOM node (nodeType), with the web's numeric values.
///
enum class NodeType : uint8_t {
  None = 0,                   ///< No node (an empty handle or one whose page is gone).
  Element = 1,                ///< An element (eg, `<div>`).
  Attribute = 2,              ///< An attribute node.
  Text = 3,                   ///< A text node.
  CDATASection = 4,           ///< A CDATA section (XML documents).
  ProcessingInstruction = 7,  ///< A processing instruction (XML documents).
  Comment = 8,                ///< A comment (`<!-- -->`).
  Document = 9,               ///< A document (never returned as a Node).
  DocumentType = 10,          ///< A doctype (eg, `<!DOCTYPE html>`).
  DocumentFragment = 11,      ///< A document fragment.
};

///
/// A reference to an item in the DOM tree.
///
/// While query methods like Document::querySelector() find only elements, Node represents any item
/// in the document hierarchy.
///
/// Use Node to walk tree relationships when your application needs to reach content that element
/// queries skip.
///
/// Consider an element containing both text and markup:
///
/// ```html
/// <p id="greeting">Howdy <b>world</b>!</p>
/// ```
///
/// You can step from the element to its first child and narrow the next sibling to an element:
///
/// ```
/// dom::Element greeting = document.getElementById("greeting");
///
/// dom::Node first = greeting.firstChild();
/// Log(first.nodeName());  // "#text" (the "Howdy " text)
///
/// dom::Element bold = first.nextSibling().AsElement();
/// Log(bold.tagName());    // "B"
/// ```
///
/// ## Kinds of Nodes
///
/// Both dom::Element and dom::DocumentFragment are Node%s. Unlike on the web, dom::Document is a
/// separate type and doesn't inherit from Node (call ownerDocument() on any node to reach its
/// document).
///
/// nodeType() returns the kind of node, and nodeName() returns its name. Call AsElement() to narrow
/// a node to a dom::Element (it returns an empty handle for text and comments).
///
/// Traversal methods differ in which node kinds they visit:
///
/// - **Node methods visit every child.** Calls like firstChild() and nextSibling() return text and
///   comments alongside elements.
/// - **Element methods visit elements only.** Calls on dom::Element like
///   Element::firstElementChild() and Element::nextElementSibling() skip text and comments.
///
/// Like other DOM handles, a Node is always Valid, Empty, or Gone (see dom::Element).
///
/// ## Text and Comment Nodes
///
/// You can create standalone text and comment nodes on the document using
/// Document::createTextNode() and Document::createComment(). Passing strings to insertion methods
/// like Element::append(), Element::prepend(), or before() creates text nodes automatically
/// (strings are never parsed as markup).
///
/// Assigning to nodeValue updates a text node in place, leaving sibling nodes untouched (unlike
/// assigning to textContent on the parent):
///
/// ```
/// dom::Element scoreboard = document.getElementById("score");
/// dom::Node points = document.createTextNode("0");
/// scoreboard.append("Score: ", points);  // the string becomes a text node too
///
/// points.nodeValue = "42";               // changes only that text node
/// ```
///
/// @note  Whitespace between HTML tags becomes a text node (firstElementChild() skips it).
///
/// @see  dom::Element, dom::DocumentFragment, dom::NodeList, dom::Document::createTextNode(),
///       dom::Document::createComment()
///
class Node {
 public:
  ///
  /// The text of a text or comment node (nodeValue). Assign to replace it.
  ///
  /// Other kinds of node (eg, elements) read as an empty string and ignore writes.
  ///
  detail::NodeStringProp<detail::NodeValueTag> nodeValue;

  ///
  /// The text of this node and all its descendants (textContent).
  ///
  /// Assigning to an element replaces its children with the text (an empty string removes them).
  ///
  detail::NodeStringProp<detail::NodeTextContentTag> textContent;

  ///
  /// Internal storage (not part of the API). Use raw() to get the C handle.
  ///
  detail::NodeStorage detail_;

  ///
  /// Create an empty Node.
  ///
  Node() {}

  ///
  /// Copy constructor (both handles refer to the same node).
  ///
  /// @param  other  The Node to copy.
  ///
  Node(const Node& other)
      : detail_ { other.detail_.handle ? ulCreateDOMNodeRef(other.detail_.handle)
                                       : nullptr } {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Node to move from.
  ///
  Node(Node&& other) noexcept : detail_ { other.detail_.handle } {
    other.detail_.handle = nullptr;
  }

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The Node to assign from.
  ///
  /// @return  Returns this Node.
  ///
  Node& operator=(Node other) noexcept {
    std::swap(detail_.handle, other.detail_.handle);
    return *this;
  }

  ///
  /// Destroy this handle (the node itself isn't affected).
  ///
  ~Node() { ulDestroyDOMNode(detail_.handle); }

  ///
  /// Whether or not this Node is valid (see IsAlive()).
  ///
  explicit operator bool() const { return IsAlive(); }

  ///
  /// Whether or not this Node is empty (it holds no handle).
  ///
  bool IsEmpty() const { return detail_.handle == nullptr; }

  ///
  /// Whether or not this Node is valid (it isn't empty and its page is still alive).
  ///
  /// @note  Safe to call from any thread.
  ///
  bool IsAlive() const { return detail_.handle && ulDOMNodeIsAlive(detail_.handle); }

  ///
  /// Whether or not two handles refer to the same node (isSameNode).
  ///
  /// @param  other  The handle to compare with.
  ///
  /// @return  Returns true if both handles are valid and refer to the same node.
  ///
  bool IsSame(const Node& other) const {
    return ulDOMNodeIsSame(detail_.handle, other.detail_.handle);
  }

  // --- Reads ------------------------------------------------------------------------------

  ///
  /// Get the kind of node (nodeType).
  ///
  /// @return  Returns the kind of node (NodeType::None for an empty Node or one whose page is
  ///          gone).
  ///
  NodeType nodeType() const {
    return static_cast<NodeType>(ulDOMNodeGetNodeType(detail_.handle));
  }

  ///
  /// Get the node's name (nodeName).
  ///
  /// @return  Returns the tag name for an element (uppercase for HTML, eg, `DIV`), `#text` for a
  ///          text node, or `#comment` for a comment.
  ///
  std::string nodeName() const {
    return detail::TakeString(ulDOMNodeGetNodeName(detail_.handle));
  }

  // --- Traversal --------------------------------------------------------------------------

  ///
  /// Get the node's parent (parentNode).
  ///
  /// @return  Returns the parent node (empty if the node has no parent or its parent is the
  ///          document).
  ///
  Node parentNode() const { return Node(ulDOMNodeGetParentNode(detail_.handle)); }

  ///
  /// Get the node's parent element (parentElement).
  ///
  /// @return  Returns the parent element (empty if the parent isn't an element).
  ///
  Element parentElement() const;

  ///
  /// Get the node's first child of any kind (firstChild).
  ///
  /// @return  Returns the first child (empty if there's none).
  ///
  Node firstChild() const { return Node(ulDOMNodeGetFirstChild(detail_.handle)); }

  ///
  /// Get the node's last child of any kind (lastChild).
  ///
  /// @return  Returns the last child (empty if there's none).
  ///
  Node lastChild() const { return Node(ulDOMNodeGetLastChild(detail_.handle)); }

  ///
  /// Get the node's previous sibling of any kind (previousSibling).
  ///
  /// @return  Returns the previous sibling (empty if there's none).
  ///
  Node previousSibling() const {
    return Node(ulDOMNodeGetPreviousSibling(detail_.handle));
  }

  ///
  /// Get the node's next sibling of any kind (nextSibling).
  ///
  /// @return  Returns the next sibling (empty if there's none).
  ///
  Node nextSibling() const { return Node(ulDOMNodeGetNextSibling(detail_.handle)); }

  ///
  /// Get the node's children of every kind in order (childNodes).
  ///
  /// This is defined in `<Ultralight/dom/NodeList.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @return  Returns the children, including text and comment nodes (an empty list if there are
  ///          none).
  ///
  /// @note  The list is a snapshot, where the web's childNodes is live. Call this again to see
  ///        later changes.
  ///
  NodeList childNodes() const;

  ///
  /// Whether or not the node has any children (hasChildNodes).
  ///
  /// @return  Returns true if the node has at least one child of any kind.
  ///
  bool hasChildNodes() const { return ulDOMNodeHasChildNodes(detail_.handle); }

  ///
  /// Whether or not the node is in its document (isConnected).
  ///
  /// A node you just created, or one you removed, isn't connected until you insert it into the
  /// page.
  ///
  /// @return  Returns true if the node is in the document (false for an empty Node or one whose
  ///          page is gone).
  ///
  bool isConnected() const { return ulDOMNodeIsConnected(detail_.handle); }

  ///
  /// Get the document this node belongs to (ownerDocument).
  ///
  /// @return  Returns the document of the page this handle came from.
  ///
  Document ownerDocument() const;

  ///
  /// Get this node as an Element.
  ///
  /// @return  Returns an Element for the same node (empty if the node isn't an element).
  ///
  Element AsElement() const;

  ///
  /// Get this node as a DocumentFragment.
  ///
  /// This is defined in `<Ultralight/dom/DocumentFragment.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @return  Returns a DocumentFragment for the same node (empty if the node isn't a document
  ///          fragment).
  ///
  DocumentFragment AsFragment() const;

  // --- Tree -------------------------------------------------------------------------------

  ///
  /// Insert nodes and strings just before this node, in its parent (before).
  ///
  /// Each string is inserted as a new text node (it's never parsed as markup). This does nothing
  /// if the node has no parent.
  ///
  /// @param  nodes  The nodes and strings to insert, in order. A node that's already in a
  ///                document moves (even if it's one of this node's siblings).
  ///
  /// @note  With more than one argument, the nodes leave their old positions before the insertion
  ///        is checked (like the web), so a call that fails can leave them out of the page.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void before(T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    ulDOMNodeBefore(detail_.handle, items.data(), items.size(), nullptr);
  }

  ///
  /// Same as before(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError for a cycle).
  ///
  /// dom::Checked comes first here, before the nodes (`node.before(dom::Checked, a, "b")`).
  ///
  /// @return  Returns success (including when this node has no parent). Fails with a
  ///          HierarchyRequestError if one of the nodes can't go there (eg, it's this node's
  ///          parent or one of its ancestors).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> before(Checked_t, T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeBefore(detail_.handle, items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Insert nodes and strings just after this node, in its parent (after).
  ///
  /// Each string is inserted as a new text node (it's never parsed as markup). This does nothing
  /// if the node has no parent.
  ///
  /// @param  nodes  The nodes and strings to insert, in order. A node that's already in a
  ///                document moves (even if it's one of this node's siblings).
  ///
  /// @note  With more than one argument, the nodes leave their old positions before the insertion
  ///        is checked (like the web), so a call that fails can leave them out of the page.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void after(T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    ulDOMNodeAfter(detail_.handle, items.data(), items.size(), nullptr);
  }

  ///
  /// Same as after(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError for a cycle).
  ///
  /// dom::Checked comes first here, before the nodes (`node.after(dom::Checked, a, "b")`).
  ///
  /// @return  Returns success (including when this node has no parent). Fails with a
  ///          HierarchyRequestError if one of the nodes can't go there (eg, it's this node's
  ///          parent or one of its ancestors).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> after(Checked_t, T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeAfter(detail_.handle, items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace this node with nodes and strings, in its parent (replaceWith).
  ///
  /// Each string is inserted as a new text node (it's never parsed as markup). With no arguments,
  /// this removes the node. This does nothing if the node has no parent. This handle still
  /// refers to the removed node afterward.
  ///
  /// @param  nodes  The nodes and strings to put in its place, in order. A node that's already in
  ///                a document moves (even if it's one of this node's siblings).
  ///
  /// @note  With more than one argument, the nodes leave their old positions before the insertion
  ///        is checked (like the web), so a call that fails can leave them out of the page.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void replaceWith(T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    ulDOMNodeReplaceWith(detail_.handle, items.data(), items.size(), nullptr);
  }

  ///
  /// Same as replaceWith(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError for a cycle).
  ///
  /// dom::Checked comes first here, before the nodes (`node.replaceWith(dom::Checked, a, "b")`).
  ///
  /// @return  Returns success (including when this node has no parent). Fails with a
  ///          HierarchyRequestError if one of the nodes can't go there (eg, it's this node's
  ///          parent or one of its ancestors).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> replaceWith(Checked_t, T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeReplaceWith(detail_.handle, items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Remove this node from its parent (remove). Does nothing if it has no parent.
  ///
  /// This handle still refers to the removed node, so you can insert it again.
  ///
  void remove() const { ulDOMNodeRemove(detail_.handle, nullptr); }

  ///
  /// Same as remove(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (including when this node has no parent). It fails only when this
  ///          Node is empty or its page is gone.
  ///
  [[nodiscard]] Result<void> remove(Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeRemove(detail_.handle, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  // --- Interop with the C API (most embedders never touch raw handles) -----------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMNode() (NULL gives an empty Node).
  ///
  /// @return  Returns a Node that destroys `handle` when it's done.
  ///
  static Node Adopt(ULDOMNode handle) { return Node(handle); }

  ///
  /// Wrap a C handle the library owns (eg, a callback argument), adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Node).
  ///
  /// @return  Returns a Node with its own reference, so you can keep it after the callback.
  ///
  static Node FromBorrowed(ULDOMNode handle) {
    return Node(handle ? ulCreateDOMNodeRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMNode.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Node). This Node still owns it, so don't
  ///          destroy it.
  ///
  ULDOMNode raw() const { return detail_.handle; }

  ///
  /// Give up ownership of the C handle and return it. This Node becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMNode() when finished.
  ///
  ULDOMNode LeakRef() {
    ULDOMNode handle = detail_.handle;
    detail_.handle = nullptr;
    return handle;
  }

 protected:
  explicit Node(ULDOMNode handle) : detail_ { handle } {}
};

// Node keeps every data member in one class, so its member offsets are well-defined
// constants for the proxy owner recovery below (Element's own members ride clang's
// conditional offsetof support instead; see the note after Element).
static_assert(std::is_standard_layout_v<Node>, "Node must stay standard-layout");
static_assert(!std::is_aggregate_v<Node>, "Node must not be an aggregate");

/// \cond INTERNAL
namespace detail {

inline ULDOMNodeOrString NodeItem(const Node& node) {
  return { node.raw(), nullptr, 0 };
}

inline ULDOMNode OwnerNodeHandle(const void* member, size_t member_offset) {
  const Node* node = reinterpret_cast<const Node*>(
      reinterpret_cast<const char*>(member) - member_offset);
  return node->detail_.handle;
}

template <>
inline size_t NodePropOffset<NodeValueTag>() {
  return offsetof(Node, nodeValue);
}
template <>
inline size_t NodePropOffset<NodeTextContentTag>() {
  return offsetof(Node, textContent);
}

template <class Tag>
void NodeStringProp<Tag>::operator=(std::string_view value) const {
  Tag::Set(OwnerNodeHandle(this, NodePropOffset<Tag>()), value);
}

template <class Tag>
void NodeStringProp<Tag>::operator+=(std::string_view suffix) const {
  ULDOMNode h = OwnerNodeHandle(this, NodePropOffset<Tag>());
  std::string combined = Tag::Get(h);
  combined.append(suffix);
  Tag::Set(h, combined);
}

template <class Tag>
NodeStringProp<Tag>::operator std::string() const {
  return Tag::Get(OwnerNodeHandle(this, NodePropOffset<Tag>()));
}

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight

#pragma pop_macro("None")
