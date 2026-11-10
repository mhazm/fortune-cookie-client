///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMNode.h>
#include <Ultralight/dom/Node.h>

#include <cstddef>
#include <utility>

namespace ultralight {
namespace dom {

///
/// A fixed list of nodes of any kind (eg, the results of Node::childNodes()).
///
/// The list is a snapshot, where the web's childNodes is live. Later changes to the page never
/// add, remove, or reorder its entries, and a node removed from the page stays in the list and
/// stays valid. Call childNodes() again when you need the current children.
///
/// ```
/// for (dom::Node child : list.childNodes()) {
///   if (child.nodeType() == dom::NodeType::Text)
///     child.remove();
/// }
/// ```
///
/// A NodeList can be moved but not copied.
///
/// The list doesn't keep its page alive. Once the page goes away, the list keeps its length but
/// its nodes fail safely (see dom::Element).
///
class NodeList {
 public:
  ///
  /// Create an empty NodeList (length() is 0).
  ///
  NodeList() = default;

  NodeList(const NodeList&) = delete;
  NodeList& operator=(const NodeList&) = delete;

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The NodeList to move from.
  ///
  NodeList(NodeList&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Move assignment (destroys the current list and `other` becomes empty).
  ///
  /// @param  other  The NodeList to move from.
  ///
  /// @return  Returns this NodeList.
  ///
  NodeList& operator=(NodeList&& other) noexcept {
    if (this != &other) {
      ulDestroyDOMNodeList(handle_);
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ///
  /// Destroy the list (nodes you got from it aren't affected).
  ///
  ~NodeList() { ulDestroyDOMNodeList(handle_); }

  ///
  /// Get the number of nodes in the list (length).
  ///
  /// @return  Returns the number of nodes (0 for an empty NodeList).
  ///
  size_t length() const { return ulDOMNodeListGetLength(handle_); }

  ///
  /// Get the number of nodes in the list (the same as length()).
  ///
  /// @return  Returns the number of nodes (0 for an empty NodeList).
  ///
  size_t size() const { return length(); }

  ///
  /// Whether or not the list has no nodes (length() is 0).
  ///
  bool empty() const { return length() == 0; }

  ///
  /// Get the node at an index (item).
  ///
  /// @param  index  The zero-based index.
  ///
  /// @return  Returns the node (empty if `index` is out of range).
  ///
  Node item(size_t index) const { return Node::Adopt(ulDOMNodeListGetNode(handle_, index)); }

  ///
  /// Get the node at an index (the same as item()).
  ///
  /// @param  index  The zero-based index.
  ///
  /// @return  Returns the node (empty if `index` is out of range).
  ///
  Node operator[](size_t index) const { return item(index); }

  ///
  /// A forward iterator over the list's nodes, so range-for works
  /// (`for (dom::Node node : list)`).
  ///
  class iterator {
   public:
    iterator(const NodeList* list, size_t index) : list_(list), index_(index) {}

    Node operator*() const { return list_->item(index_); }
    iterator& operator++() {
      ++index_;
      return *this;
    }
    bool operator!=(const iterator& other) const { return index_ != other.index_; }
    bool operator==(const iterator& other) const { return index_ == other.index_; }

   private:
    const NodeList* list_;
    size_t index_;
  };

  iterator begin() const { return iterator(this, 0); }
  iterator end() const { return iterator(this, length()); }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMNodeList() (NULL gives an empty NodeList).
  ///
  /// @return  Returns a NodeList that destroys `handle` when it's done.
  ///
  static NodeList Adopt(ULDOMNodeList handle) { return NodeList(handle); }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMNode.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty NodeList). This NodeList still owns it, so
  ///          don't destroy it.
  ///
  ULDOMNodeList raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This NodeList becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMNodeList() when finished.
  ///
  ULDOMNodeList LeakRef() {
    ULDOMNodeList handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit NodeList(ULDOMNodeList handle) : handle_(handle) {}

 private:
  ULDOMNodeList handle_ = nullptr;
};

inline NodeList Node::childNodes() const {
  return NodeList::Adopt(ulDOMNodeGetChildNodes(detail_.handle));
}

}  // namespace dom
}  // namespace ultralight
