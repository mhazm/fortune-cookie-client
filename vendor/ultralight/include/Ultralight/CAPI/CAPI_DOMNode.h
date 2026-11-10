///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMNode.h
///
/// DOM nodes and tree traversal.
///
/// `#include <Ultralight/CAPI/CAPI_DOMNode.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// While query functions like ulDOMDocumentQuerySelector() find only elements, a ULDOMNode
/// represents any item in the document tree, including text and comments.
///
/// You'll use ULDOMNode to navigate the document hierarchy.
///
/// Consider an element containing both text and markup:
///
/// ```html
/// <p id="greeting">Howdy <b>world</b>!</p>
/// ```
///
/// You can step from the element to its first child and narrow the next sibling to an element:
///
/// ```c
/// void ShowGreeting(ULDOMDocument document) {
///   ULDOMElement greeting = ulDOMDocumentGetElementById(document, "greeting");
///   ULDOMNode greeting_node = ulDOMElementAsNode(greeting);
///
///   ULDOMNode first = ulDOMNodeGetFirstChild(greeting_node);  // the "Howdy " text
///   ULDOMNode second = ulDOMNodeGetNextSibling(first);
///   ULDOMElement bold = ulDOMNodeAsElement(second);           // the <b> element
///
///   ulDestroyDOMElement(bold);
///   ulDestroyDOMNode(second);
///   ulDestroyDOMNode(first);
///   ulDestroyDOMNode(greeting_node);
///   ulDestroyDOMElement(greeting);
/// }
/// ```
///
/// ## Nodes and Elements
///
/// Both ULDOMElement and ULDOMFragment are nodes. Unlike on the web, ULDOMDocument is a separate
/// type rather than a node (call ulDOMNodeGetOwnerDocument() on any node to reach its document).
///
/// Call ulDOMElementAsNode() or ulDOMFragmentAsNode() to get a node handle for an element or
/// fragment. Call ulDOMNodeAsElement() to narrow a node to a ULDOMElement (it returns NULL for text
/// and comments). Converting a handle adds a reference, so you must destroy both the original
/// handle and the converted handle when finished.
///
/// Traversal functions differ in which kinds of child node they visit:
///
/// - **Node functions return children of any kind.** Functions like ulDOMNodeGetFirstChild() and
///   ulDOMNodeGetNextSibling() return text and comments alongside elements.
/// - **Element functions return child elements only.** Functions on ULDOMElement like
///   ulDOMElementGetFirstElementChild() skip text and comments.
///
/// ## Text and Comment Nodes
///
/// You can create standalone text and comment nodes on the document using
/// ulDOMDocumentCreateTextNode() and ulDOMDocumentCreateComment(). Functions that insert multiple
/// items, such as ulDOMElementAppend() or ulDOMNodeBefore(), accept an array of ULDOMNodeOrString
/// structures holding any mix of nodes and strings (text is never parsed as markup).
///
/// Calling ulDOMNodeSetNodeValue() updates a text node in place, leaving sibling nodes untouched
/// (unlike calling ulDOMNodeSetTextContent() or ulDOMElementSetTextContent() on the parent, which
/// replaces all children):
///
/// ```c
/// void ShowScore(ULDOMDocument document) {
///   ULDOMElement scoreboard = ulDOMDocumentGetElementById(document, "score");
///   ULDOMNode points = ulDOMDocumentCreateTextNode(document, "0", 1);
///
///   ULDOMNodeOrString items[] = {
///     { NULL, "Score: ", 7 },  // the text becomes a new text node
///     { points, NULL, 0 },     // a node
///   };
///   ulDOMElementAppend(scoreboard, items, 2, NULL);
///
///   ulDOMNodeSetNodeValue(points, "42", 2);  // changes only that text node
///
///   ulDestroyDOMNode(points);
///   ulDestroyDOMElement(scoreboard);
/// }
/// ```
///
/// For shared rules on DOM handle lifetimes, error reporting with ULDOMError, and threading, see
/// `<Ultralight/CAPI/CAPI_DOMDocument.h>`.
///
/// @note  Whitespace between HTML tags becomes a text node (ulDOMElementGetFirstElementChild()
///        skips it).
///
/// @see  ulDOMElementAsNode(), ulDOMNodeGetChildNodes(), ulDOMElementAppend(),
///       ulDOMDocumentCreateTextNode(), ULDOMNodeOrString
///
#ifndef ULTRALIGHT_CAPI_DOMNODE_H
#define ULTRALIGHT_CAPI_DOMNODE_H

#include <Ultralight/CAPI/CAPI_DOMDocument.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// The kinds of DOM node (nodeType), with the web's numeric values.
///
typedef enum {
  kULDOMNodeType_None = 0,                   ///< No node (a NULL handle or one whose page is gone).
  kULDOMNodeType_Element = 1,                ///< An element (eg, `<div>`).
  kULDOMNodeType_Attribute = 2,              ///< An attribute node.
  kULDOMNodeType_Text = 3,                   ///< A text node.
  kULDOMNodeType_CDATASection = 4,           ///< A CDATA section (XML documents).
  kULDOMNodeType_ProcessingInstruction = 7,  ///< A processing instruction (XML documents).
  kULDOMNodeType_Comment = 8,                ///< A comment (`<!-- -->`).
  kULDOMNodeType_Document = 9,               ///< A document (never returned as a ULDOMNode).
  kULDOMNodeType_DocumentType = 10,          ///< A doctype (eg, `<!DOCTYPE html>`).
  kULDOMNodeType_DocumentFragment = 11,      ///< A document fragment.
} ULDOMNodeType;

///
/// Add a reference to a node handle.
///
/// @param  node  The node handle.
///
/// @return  Returns `node` (NULL if `node` is NULL). You must call ulDestroyDOMNode() when
///          finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMNode ulCreateDOMNodeRef(ULDOMNode node);

///
/// Destroy a node handle (the node itself isn't affected).
///
/// @param  node  The node handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMNode(ULDOMNode node);

///
/// Whether or not a node handle is valid (it isn't NULL and its page is still alive).
///
/// @param  node  The node handle.
///
/// @return  Returns true if the node's page is still alive (even if the node was removed from the
///          document).
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMNodeIsAlive(ULDOMNode node);

///
/// Whether or not two handles refer to the same node (isSameNode).
///
/// @param  node   The first node handle.
///
/// @param  other  The node handle to compare with.
///
/// @return  Returns true if both handles are valid and refer to the same node.
///
ULExport bool ulDOMNodeIsSame(ULDOMNode node, ULDOMNode other);

///
/// Get a node handle for an element (every element is also a node).
///
/// The result is the same handle with a new reference, so ulDestroyDOMNode() and
/// ulDestroyDOMElement() both work on it.
///
/// @param  element  The element handle.
///
/// @return  Returns a node handle (NULL if `element` is NULL). You must call ulDestroyDOMNode()
///          when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMNode ulDOMElementAsNode(ULDOMElement element);

///
/// Get an element handle for a node.
///
/// The result is the same handle with a new reference, so ulDestroyDOMElement() and
/// ulDestroyDOMNode() both work on it.
///
/// @param  node  The node handle.
///
/// @return  Returns an element handle, or NULL if the node isn't an element (eg, a text node).
///          You must call ulDestroyDOMElement() when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMElement ulDOMNodeAsElement(ULDOMNode node);

///
/// Get a node handle for a document fragment (every fragment is also a node).
///
/// The result is the same handle with a new reference, so ulDestroyDOMNode() and
/// ulDestroyDOMFragment() both work on it. Pass it to the node functions to insert the fragment
/// or read its children.
///
/// @param  fragment  The fragment handle.
///
/// @return  Returns a node handle (NULL if `fragment` is NULL). You must call ulDestroyDOMNode()
///          when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMNode ulDOMFragmentAsNode(ULDOMFragment fragment);

///
/// Get a fragment handle for a node.
///
/// The result is the same handle with a new reference, so ulDestroyDOMFragment() and
/// ulDestroyDOMNode() both work on it.
///
/// @param  node  The node handle.
///
/// @return  Returns a fragment handle, or NULL if the node isn't a document fragment (eg, an
///          element). You must call ulDestroyDOMFragment() when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMFragment ulDOMNodeAsFragment(ULDOMNode node);

///
/// Get the kind of node (nodeType).
///
/// @param  node  The node handle.
///
/// @return  Returns the kind of node (kULDOMNodeType_None for a NULL handle or one whose page
///          is gone).
///
ULExport ULDOMNodeType ulDOMNodeGetNodeType(ULDOMNode node);

///
/// Get the node's name (nodeName).
///
/// @param  node  The node handle.
///
/// @return  Returns the tag name for an element (uppercase for HTML, eg, `DIV`), `#text` for a
///          text node, or `#comment` for a comment. You must call ulDestroyString() when
///          finished.
///
ULExport ULString ulDOMNodeGetNodeName(ULDOMNode node);

///
/// Get the text of a text or comment node (nodeValue).
///
/// @param  node  The node handle.
///
/// @return  Returns the text, or NULL for other kinds of node (eg, elements). You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMNodeGetNodeValue(ULDOMNode node);

///
/// Set the text of a text or comment node (nodeValue). Other kinds of node ignore this.
///
/// @param  node    The node handle.
///
/// @param  value   The new text, as UTF-8.
///
/// @param  length  The length of `value`, in bytes.
///
ULExport void ulDOMNodeSetNodeValue(ULDOMNode node, const char* value, size_t length);

///
/// Get the text of a node and all its descendants (textContent).
///
/// @param  node  The node handle.
///
/// @return  Returns the text. You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMNodeGetTextContent(ULDOMNode node);

///
/// Set the text of a node (textContent).
///
/// For an element this replaces its children with the text (empty text removes them). For a text
/// or comment node it replaces the node's text.
///
/// @param  node    The node handle.
///
/// @param  text    The text, as UTF-8.
///
/// @param  length  The length of `text`, in bytes.
///
ULExport void ulDOMNodeSetTextContent(ULDOMNode node, const char* text, size_t length);

///
/// Get the node's parent (parentNode).
///
/// @param  node  The node handle.
///
/// @return  Returns the parent, or NULL if the node has no parent or its parent is the document
///          (use ulDOMNodeGetOwnerDocument() for that). You must call ulDestroyDOMNode() when
///          finished.
///
ULExport ULDOMNode ulDOMNodeGetParentNode(ULDOMNode node);

///
/// Get the node's parent element (parentElement).
///
/// @param  node  The node handle.
///
/// @return  Returns the parent element, or NULL if the parent isn't an element. You must call
///          ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMNodeGetParentElement(ULDOMNode node);

///
/// Get the node's first child of any kind (firstChild).
///
/// @param  node  The node handle.
///
/// @return  Returns the first child, or NULL if there's none. You must call ulDestroyDOMNode()
///          when finished.
///
ULExport ULDOMNode ulDOMNodeGetFirstChild(ULDOMNode node);

///
/// Get the node's last child of any kind (lastChild).
///
/// @param  node  The node handle.
///
/// @return  Returns the last child, or NULL if there's none. You must call ulDestroyDOMNode()
///          when finished.
///
ULExport ULDOMNode ulDOMNodeGetLastChild(ULDOMNode node);

///
/// Get the node's previous sibling of any kind (previousSibling).
///
/// @param  node  The node handle.
///
/// @return  Returns the previous sibling, or NULL if there's none. You must call
///          ulDestroyDOMNode() when finished.
///
ULExport ULDOMNode ulDOMNodeGetPreviousSibling(ULDOMNode node);

///
/// Get the node's next sibling of any kind (nextSibling).
///
/// @param  node  The node handle.
///
/// @return  Returns the next sibling, or NULL if there's none. You must call ulDestroyDOMNode()
///          when finished.
///
ULExport ULDOMNode ulDOMNodeGetNextSibling(ULDOMNode node);

///
/// Get the document this node belongs to (ownerDocument).
///
/// @param  node  The node handle.
///
/// @return  Returns the document of the page this handle came from. You must call
///          ulDestroyDOMDocument() when finished.
///
ULExport ULDOMDocument ulDOMNodeGetOwnerDocument(ULDOMNode node);

///
/// Whether or not the node has any children (hasChildNodes).
///
/// @param  node  The node handle.
///
/// @return  Returns true if the node has at least one child of any kind.
///
ULExport bool ulDOMNodeHasChildNodes(ULDOMNode node);

///
/// Whether or not the node is in its document (isConnected).
///
/// A node you just created, or one you removed, isn't connected until you insert it into the page.
///
/// @param  node  The node handle.
///
/// @return  Returns true if the node is in the document (false for a NULL handle or one whose page
///          is gone).
///
ULExport bool ulDOMNodeIsConnected(ULDOMNode node);

///
/// Get the node's children of every kind in order (childNodes).
///
/// @param  node  The node handle.
///
/// @return  Returns a list of the children, including text and comment nodes (empty if there are
///          none). You must call ulDestroyDOMNodeList() when finished.
///
/// @note  Unlike the web's live list, the list doesn't change when the document does. Call this
///        again to see later changes.
///
ULExport ULDOMNodeList ulDOMNodeGetChildNodes(ULDOMNode node);

///
/// Destroy a node list.
///
/// Node handles you got from the list with ulDOMNodeListGetNode() aren't affected.
///
/// @param  list  The list (can be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMNodeList(ULDOMNodeList list);

///
/// Get the number of nodes in a list.
///
/// @param  list  The list.
///
/// @return  Returns the number of nodes.
///
/// @note  Safe to call from any thread.
///
ULExport size_t ulDOMNodeListGetLength(ULDOMNodeList list);

///
/// Get a node from a list.
///
/// @param  list   The list.
///
/// @param  index  The index of the node.
///
/// @return  Returns the node, or NULL if `index` is out of range. You must call ulDestroyDOMNode()
///          when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMNode ulDOMNodeListGetNode(ULDOMNodeList list, size_t index);

///
/// Add a node as the last child of another node (appendChild).
///
/// If `child` is already in a document, it moves from its old position. If `child` is a document
/// fragment, all of its children move instead.
///
/// @param  parent  The node to add to.
///
/// @param  child   The node to add.
///
/// @param  error   Receives kULDOMErrorCode_HierarchyRequestError if `child` can't go there (eg,
///                 `child` is `parent` or one of its ancestors, or `parent` is a text node) (can
///                 be NULL).
///
/// @return  Returns true on success (the web returns `child`, which you already hold).
///
/// @note  A handle belongs to the page it came from. If you move a node here from another
///        document, the handles you already had to it still stop working when that document's
///        page goes away.
///
ULExport bool ulDOMNodeAppendChild(ULDOMNode parent, ULDOMNode child, ULDOMError* error);

///
/// Insert a node before one of another node's children (insertBefore).
///
/// @param  parent     The node to insert into.
///
/// @param  child      The node to insert. If it's already in a document, it moves.
///
/// @param  ref_child  The child to insert before. Pass NULL to add `child` at the end.
///
/// @param  error      Receives kULDOMErrorCode_NotFoundError if `ref_child` isn't a child of
///                    `parent`, or kULDOMErrorCode_HierarchyRequestError if `child` can't go
///                    there (eg, `child` is `parent` or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMNodeInsertBefore(ULDOMNode parent, ULDOMNode child, ULDOMNode ref_child,
                                    ULDOMError* error);

///
/// Replace one of a node's children with another node (replaceChild).
///
/// @param  parent     The node whose child to replace.
///
/// @param  new_child  The node to put in its place (it comes first, as on the web). If it's
///                    already in a document, it moves.
///
/// @param  old_child  The child to replace. Its handles still work after it's removed.
///
/// @param  error      Receives kULDOMErrorCode_NotFoundError if `old_child` isn't a child of
///                    `parent`, or kULDOMErrorCode_HierarchyRequestError if `new_child` can't go
///                    there (eg, `new_child` is `parent` or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMNodeReplaceChild(ULDOMNode parent, ULDOMNode new_child, ULDOMNode old_child,
                                    ULDOMError* error);

///
/// Remove one of a node's children (removeChild).
///
/// @param  parent  The node whose child to remove.
///
/// @param  child   The child to remove. Its handles still work after it's removed.
///
/// @param  error   Receives kULDOMErrorCode_NotFoundError if `child` isn't a child of `parent`
///                 (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMNodeRemoveChild(ULDOMNode parent, ULDOMNode child, ULDOMError* error);

///
/// Remove the node from its parent (remove). Does nothing if it has no parent.
///
/// @param  node   The node handle.
///
/// @param  error  Receives a DOM error if the removal fails (can be NULL).
///
/// @return  Returns true on success (including when the node has no parent).
///
ULExport bool ulDOMNodeRemove(ULDOMNode node, ULDOMError* error);

///
/// One node or piece of text to insert, for the functions that insert several at once (eg,
/// ulDOMElementAppend()).
///
/// Set `node` to insert that node. Set `node` to NULL to insert `text` as a new text node (it's
/// never parsed as markup). An item with neither makes the call fail.
///
typedef struct {
  ULDOMNode node;    ///< The node to insert, or NULL to insert `text` instead.
  const char* text;  ///< The text to insert, as UTF-8 (used only when `node` is NULL).
  size_t length;     ///< The length of `text`, in bytes.
} ULDOMNodeOrString;

///
/// Insert nodes and text just before this node, in its parent (before).
///
/// Does nothing if the node has no parent.
///
/// @param  node   The node handle.
///
/// @param  items  The nodes and text to insert in order. A node that's already in a document
///                moves (even if it's one of this node's siblings).
///
/// @param  count  The number of items in `items`.
///
/// @param  error  Receives kULDOMErrorCode_HierarchyRequestError if an item can't go there (eg,
///                it's the parent or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success (including when the node has no parent).
///
/// @note  With more than one item, the nodes leave their old positions before the insertion is
///        checked (like the web), so a call that fails can leave them out of the page.
///
ULExport bool ulDOMNodeBefore(ULDOMNode node, const ULDOMNodeOrString* items, size_t count,
                              ULDOMError* error);

///
/// Insert nodes and text just after this node, in its parent (after).
///
/// Does nothing if the node has no parent.
///
/// @param  node   The node handle.
///
/// @param  items  The nodes and text to insert in order. A node that's already in a document
///                moves (even if it's one of this node's siblings).
///
/// @param  count  The number of items in `items`.
///
/// @param  error  Receives kULDOMErrorCode_HierarchyRequestError if an item can't go there (eg,
///                it's the parent or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success (including when the node has no parent).
///
/// @note  With more than one item, the nodes leave their old positions before the insertion is
///        checked (like the web), so a call that fails can leave them out of the page.
///
ULExport bool ulDOMNodeAfter(ULDOMNode node, const ULDOMNodeOrString* items, size_t count,
                             ULDOMError* error);

///
/// Replace this node with nodes and text, in its parent (replaceWith).
///
/// Does nothing if the node has no parent. With no items (a `count` of 0), this removes the node.
///
/// @param  node   The node handle. Its handles still work after it's removed.
///
/// @param  items  The nodes and text to put in its place, in order. A node that's already in a
///                document moves (even if it's one of this node's siblings, or the node itself).
///
/// @param  count  The number of items in `items`.
///
/// @param  error  Receives kULDOMErrorCode_HierarchyRequestError if an item can't go there (eg,
///                it's the parent or one of its ancestors) (can be NULL).
///
/// @return  Returns true on success (including when the node has no parent).
///
/// @note  With more than one item, the nodes leave their old positions before the insertion is
///        checked (like the web), so a call that fails can leave them out of the page.
///
ULExport bool ulDOMNodeReplaceWith(ULDOMNode node, const ULDOMNodeOrString* items, size_t count,
                                   ULDOMError* error);

///
/// Add nodes and text to the end of a fragment (append).
///
/// @param  fragment  The fragment handle.
///
/// @param  items     The nodes and text to add in order. A node that's already in a document (or
///                   another fragment) moves.
///
/// @param  count     The number of items in `items`.
///
/// @param  error     Receives kULDOMErrorCode_HierarchyRequestError if an item can't go in a
///                   fragment (eg, a doctype) (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMFragmentAppend(ULDOMFragment fragment, const ULDOMNodeOrString* items,
                                  size_t count, ULDOMError* error);

///
/// Add nodes and text to the start of a fragment, before its first child (prepend).
///
/// @param  fragment  The fragment handle.
///
/// @param  items     The nodes and text to add in order. A node that's already in a document (or
///                   another fragment) moves.
///
/// @param  count     The number of items in `items`.
///
/// @param  error     Receives kULDOMErrorCode_HierarchyRequestError if an item can't go in a
///                   fragment (eg, a doctype) (can be NULL).
///
/// @return  Returns true on success.
///
ULExport bool ulDOMFragmentPrepend(ULDOMFragment fragment, const ULDOMNodeOrString* items,
                                   size_t count, ULDOMError* error);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMNODE_H
