///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMElement.h>
#include <Ultralight/dom/Element.h>

#include <cstddef>
#include <utility>

namespace ultralight {
namespace dom {

///
/// A fixed list of DOM elements returned by a query or an Element::children() call.
///
/// Calls like dom::Document::querySelectorAll() and Element::children() return an ElementList to
/// hold matching elements from a page. You use it in native code to inspect and update groups of
/// elements, such as rows in a table or controls in a form.
///
/// Querying a table for rows and iterating over the results looks like this:
///
/// ```
/// dom::ElementList scores = document.querySelectorAll("#scores tr");
/// if (scores.empty())
///   Log("no scores yet");
///
/// for (dom::Element score_row : scores)
///   score_row.classList.add("visible");
/// ```
///
/// ## Snapshot Behavior
///
/// An ElementList is a snapshot of elements at the time of the query, unlike live collections on
/// the web. Later changes to the page never add, remove, or reorder entries in the list. Call
/// dom::Document::querySelectorAll() again when you need the current elements.
///
/// Removing an element from the page leaves it in the list, and its handle stays valid. Collections
/// like Element::children(), HTMLSelectElement::selectedOptions(), and HTMLFormElement::elements()
/// are live on the web, but return snapshots in the library.
///
/// ## Usage and Lifetime
///
/// - **Check for no results with empty() or length().** An ElementList has no boolean operator, so
///   writing `if (list)` doesn't compile.
/// - **An ElementList can't be copied.** Transfer ownership with `std::move` when storing a list in
///   a member variable.
/// - **An ElementList doesn't keep its page alive.** When the page navigates away, the list keeps
///   its length, but operations on its Element%s fail safely (see dom::Element).
///
/// @see  dom::Document::querySelectorAll(), dom::Element::querySelectorAll(),
///       dom::Element::children(), dom::NodeList
///
class ElementList {
 public:
  ///
  /// Create an empty ElementList (length() is 0).
  ///
  ElementList() = default;

  ElementList(const ElementList&) = delete;
  ElementList& operator=(const ElementList&) = delete;

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The ElementList to move from.
  ///
  ElementList(ElementList&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  ///
  /// Move assignment (destroys the current list and `other` becomes empty).
  ///
  /// @param  other  The ElementList to move from.
  ///
  /// @return  Returns this ElementList.
  ///
  ElementList& operator=(ElementList&& other) noexcept {
    if (this != &other) {
      ulDestroyDOMElementList(handle_);
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ///
  /// Destroy the list (elements you got from it aren't affected).
  ///
  ~ElementList() { ulDestroyDOMElementList(handle_); }

  ///
  /// Get the number of elements in the list (length).
  ///
  /// @return  Returns the number of elements (0 for an empty ElementList).
  ///
  size_t length() const { return ulDOMElementListGetLength(handle_); }

  ///
  /// Get the number of elements in the list (the same as length()).
  ///
  /// @return  Returns the number of elements (0 for an empty ElementList).
  ///
  size_t size() const { return length(); }

  ///
  /// Whether or not the list has no elements (length() is 0).
  ///
  bool empty() const { return length() == 0; }

  ///
  /// Get the element at an index (item).
  ///
  /// @param  index  The zero-based index.
  ///
  /// @return  Returns the element (empty if `index` is out of range).
  ///
  Element item(size_t index) const {
    return Element::Adopt(ulDOMElementListGetElement(handle_, index));
  }

  ///
  /// Get the element at an index (the same as item()).
  ///
  /// @param  index  The zero-based index.
  ///
  /// @return  Returns the element (empty if `index` is out of range).
  ///
  Element operator[](size_t index) const { return item(index); }

  ///
  /// A forward iterator over the list's elements, so range-for works
  /// (`for (dom::Element el : list)`).
  ///
  class iterator {
   public:
    iterator(const ElementList* list, size_t index) : list_(list), index_(index) {}

    Element operator*() const { return list_->item(index_); }
    iterator& operator++() {
      ++index_;
      return *this;
    }
    bool operator!=(const iterator& other) const { return index_ != other.index_; }
    bool operator==(const iterator& other) const { return index_ == other.index_; }

   private:
    const ElementList* list_;
    size_t index_;
  };

  iterator begin() const { return iterator(this, 0); }
  iterator end() const { return iterator(this, length()); }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMElementList() (NULL gives an empty ElementList).
  ///
  /// @return  Returns an ElementList that destroys `handle` when it's done.
  ///
  static ElementList Adopt(ULDOMElementList handle) { return ElementList(handle); }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMElement.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty ElementList). This ElementList still owns it,
  ///          so don't destroy it.
  ///
  ULDOMElementList raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This ElementList becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMElementList() when finished.
  ///
  ULDOMElementList LeakRef() {
    ULDOMElementList handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit ElementList(ULDOMElementList handle) : handle_(handle) {}

 private:
  ULDOMElementList handle_ = nullptr;
};

inline ElementList Element::querySelectorAll(std::string_view selectors) const {
  detail::CString s(selectors);
  return ElementList::Adopt(ulDOMElementQuerySelectorAll(raw(), s.c_str(), nullptr));
}

inline Result<ElementList> Element::querySelectorAll(std::string_view selectors,
                                                     Checked_t) const {
  if (IsEmpty())
    return detail::EmptyHandleError();
  detail::CString s(selectors);
  detail::ErrorScope error;
  ULDOMElementList result
      = ulDOMElementQuerySelectorAll(raw(), s.c_str(), error.out());
  if (result)
    return ElementList::Adopt(result);
  return Unexpected<Error>(error.TakeError());
}

inline ElementList HTMLSelectElement::selectedOptions() const {
  ULDOMElementList result = ulDOMElementGetSelectedOptions(raw());
  return result ? ElementList::Adopt(result) : ElementList();
}

inline ElementList HTMLFormElement::elements() const {
  ULDOMElementList result = ulDOMElementGetElements(raw());
  return result ? ElementList::Adopt(result) : ElementList();
}

inline ElementList Element::children() const {
  ULDOMElementList result = ulDOMElementGetChildren(raw());
  return result ? ElementList::Adopt(result) : ElementList();
}

}  // namespace dom
}  // namespace ultralight
