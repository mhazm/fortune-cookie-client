///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/String.h>

namespace ultralight {

namespace detail {
struct JSONData;
}

class JSONObject;
class JSONArray;

///
/// JSON document or value for structured data.
///
/// You use JSON to work with structured data. You can parse a document from text or build one in
/// code. Operations never throw exceptions, so missing keys or syntax errors won't crash your
/// application.
///
/// Call JSON::Parse() to parse JSON text and check for syntax errors:
///
/// ```
/// JSON prefs = JSON::Parse(json_text);
/// if (!prefs)
///   printf("line %u: %s\n", prefs.error_line(),
///          prefs.error_message().utf8().data());
/// ```
///
/// ## Reading Values
///
/// You can chain operator[] across keys or indices without checking each step. A missing key, an
/// out-of-range index, or the wrong kind of value anywhere in the chain safely resolves to the
/// fallback passed to Or().
///
/// Read nested values with fallback defaults:
///
/// ```
/// void ApplyPrefs(const JSON& prefs) {
///   double ui_scale = prefs["ui"]["scale"].Or(1.0);  // 1.0 if missing
///   String player_name = prefs["profile"]["name"].Or("anonymous");
///   bool muted = prefs["audio"]["muted"].Or(false);
/// }
/// ```
///
/// The Or() method reads a value matching the fallback's type without converting between types
/// (reading `"3"` with a fallback of `0` returns `0`). An integer fallback truncates a fractional
/// number toward zero.
///
/// Always read values through a const JSON reference. Indexing a non-const document creates missing
/// objects along the path automatically.
///
/// @note  Testing a lookup in a conditional checks whether the key exists rather than its boolean
///        value. A key holding `false` still evaluates to `true` in a conditional. Pass `false` to
///        Or() to read a JSON boolean.
///
/// ## Iterating Objects and Arrays
///
/// Iterate over an object's members using a range-for loop over AsObject():
///
/// ```
/// void ConnectAll(const JSON& prefs) {
///   for (auto [server, address] : prefs["servers"].AsObject())
///     Connect(server, address.Or(""));
/// }
/// ```
///
/// Call AsArray() to iterate over array elements (see JSONArray). See JSONObject for object
/// iteration.
///
/// ## Building Documents
///
/// Start a new document by calling JSON::Object() or JSON::Array(). Indexing a non-const document
/// with operator[] returns an assignable reference (see JSON::Ref). To build an array, assign
/// JSON::Array() before indexing elements, or call Push() through AsArray() to append items.
///
/// Populate nested values and serialize the document to text:
///
/// ```
/// JSON save = JSON::Object();
/// save["player"]["name"] = "ada";  // creates "player" first
/// save["player"]["level"] = 12;
///
/// save["unlocks"] = JSON::Array();
/// save["unlocks"][0] = "tutorial_done";
///
/// JSON unlocks = save["unlocks"];
/// unlocks.AsArray().Push("first_boss");
///
/// String saved_text = save.Stringify(2);  // two spaces per level
/// ```
///
/// ## Copying Documents
///
/// A copy shares the document, and a JSON assigned into a document shares its value as well. A
/// change made through one copy shows up in every other copy and in any document the value was
/// assigned into.
///
/// Copy a document to share it, or call Clone() to make an independent copy:
///
/// ```
/// JSON shared = save;        // the same document
/// JSON copy = save.Clone();  // an independent copy
/// ```
///
/// @warning  Only one thread at a time may access a document. Because copies share the underlying
///           data, accessing copies across threads requires your own synchronization.
///
/// @see  JSON::Ref, JSONObject, JSONArray
///
class UExport JSON {
 public:
  ///
  /// Create an empty, invalid JSON value.
  ///
  /// It has no error message.
  ///
  /// You can assign another JSON to it later.
  ///
  JSON();

  ///
  /// Create a copy of another JSON.
  ///
  /// The copy shares the same document (see the class overview).
  ///
  /// @note  Use Clone() for an independent copy.
  ///
  JSON(const JSON& other);

  ///
  /// Move constructor.
  ///
  /// The moved-from handle becomes empty and invalid.
  ///
  JSON(JSON&& other);

  ///
  /// Destructor.
  ///
  ~JSON();

  ///
  /// Assignment operator.
  ///
  /// This JSON now shares `other`'s value.
  ///
  /// @note  This doesn't write into the value this JSON referred to before-- use
  ///        `json[...] = value` instead.
  ///
  JSON& operator=(const JSON& other);

  ///
  /// Move assignment operator.
  ///
  JSON& operator=(JSON&& other);

  ///
  /// Parse JSON text.
  ///
  /// The top level accepts any JSON value (object, array, string, number, boolean, or null).
  ///
  /// @param  text  The JSON text to parse (UTF-8).
  ///
  /// @return  Returns the parsed document. On failure, is_valid() returns false, and
  ///          error_message() and error_line() describe the first syntax error.
  ///
  /// @note  Input nested deeper than 1000 levels or larger than 256 MB fails to parse.
  ///
  static JSON Parse(const String& text);

  ///
  /// Create a new empty object value.
  ///
  static JSON Object();

  ///
  /// Create a new empty array value.
  ///
  static JSON Array();

  ///
  /// Whether or not this JSON holds a value.
  ///
  /// This returns false for a failed parse, a default-constructed JSON, a moved-from JSON, or a
  /// lookup that found nothing.
  ///
  bool is_valid() const;

  ///
  /// Whether or not this JSON holds a value (the same as is_valid()).
  ///
  /// @note  This tests whether a value is present, not the JSON boolean value (see the class
  ///        overview).
  ///
  explicit operator bool() const { return is_valid(); }

  ///
  /// Get the parse error description ("" unless this is a failed Parse() result).
  ///
  String error_message() const;

  ///
  /// Get the 1-based line number of the parse error (0 unless this is a failed Parse()
  /// result).
  ///
  unsigned error_line() const;

  ///
  /// Whether or not this value is an object.
  ///
  bool is_object() const;

  ///
  /// Whether or not this value is an array.
  ///
  bool is_array() const;

  ///
  /// Whether or not this value is a string.
  ///
  bool is_string() const;

  ///
  /// Whether or not this value is a number.
  ///
  bool is_number() const;

  ///
  /// Whether or not this value is a boolean.
  ///
  bool is_bool() const;

  ///
  /// Whether or not this value is null.
  ///
  bool is_null() const;

  ///
  /// Read an object member.
  ///
  /// You can index the result again without checking it first (see the class overview).
  ///
  /// @param  key  The member name.
  ///
  /// @return  Returns a JSON sharing the member's value. A missing key, a non-object value, or an
  ///          invalid JSON returns an invalid JSON.
  ///
  JSON operator[](const String& key) const;

  ///
  /// Read an array element.
  ///
  /// @param  index  The element index.
  ///
  /// @return  Returns a JSON sharing the element's value. An out-of-range index, a non-array value,
  ///          or an invalid JSON returns an invalid JSON.
  ///
  JSON operator[](size_t index) const;

  class Ref;

  ///
  /// Get an assignable reference to an object member.
  ///
  /// Assigning through the reference writes the member.
  ///
  /// Chaining `[]` creates missing members as objects along the path.
  ///
  /// @param  key  The member name.
  ///
  /// @return  Returns a reference to the member (see JSON::Ref).
  ///
  /// @note  On a non-object value, writes through the reference do nothing.
  ///
  Ref operator[](const String& key);

  ///
  /// Get an assignable reference to an array element.
  ///
  /// Writing past the end pads the array with nulls, up to 65536 elements past the end.
  ///
  /// Writes further out are ignored.
  ///
  /// @param  index  The element index.
  ///
  /// @return  Returns a reference to the element (see JSON::Ref).
  ///
  /// @note  On a non-array value, writes through the reference do nothing.
  ///
  Ref operator[](size_t index);

  ///
  /// Read this value as a number.
  ///
  /// @param  fallback  The value to return when this value is not a number.
  ///
  /// @return  Returns the number or `fallback`.
  ///
  double Or(double fallback) const;

  ///
  /// Read this value as a number, truncated toward zero to an integer.
  ///
  /// @param  fallback  The value to return when this value is not a number.
  ///
  /// @return  Returns the truncated number or `fallback`. Numbers beyond the 64-bit range
  ///          clamp to the nearest representable integer. NaN reads as 0.
  ///
  int64_t Or(long long fallback) const;

  ///
  /// Same as Or(long long).
  ///
  int64_t Or(int fallback) const;

  ///
  /// Same as Or(long long).
  ///
  int64_t Or(unsigned fallback) const;

  ///
  /// Same as Or(long long).
  ///
  int64_t Or(long fallback) const;

  ///
  /// Same as Or(long long).
  ///
  int64_t Or(unsigned long fallback) const;

  ///
  /// Same as Or(long long).
  ///
  int64_t Or(unsigned long long fallback) const;

  ///
  /// Same as Or(double).
  ///
  double Or(float fallback) const;

  ///
  /// Read this value as a boolean.
  ///
  /// @param  fallback  The value to return when this value is not a boolean.
  ///
  /// @return  Returns the boolean or `fallback`.
  ///
  bool Or(bool fallback) const;

  ///
  /// Read this value as a string.
  ///
  /// @param  fallback  The value to return when this value is not a string.
  ///
  /// @return  Returns the string or `fallback`.
  ///
  String Or(const String& fallback) const;

  ///
  /// Same as Or(const String&).
  ///
  String Or(const char* fallback) const;

  ///
  /// An object member yielded by JSONObject iteration.
  ///
  /// Supports structured bindings (`for (auto [key, value] : json.AsObject())`).
  ///
  struct Member;

  ///
  /// Get a typed view of this value as an object (for iteration, Has(), and size()).
  ///
  /// @return  Returns the view. A non-object value or an invalid JSON returns an empty view.
  ///
  JSONObject AsObject() const;

  ///
  /// Get a typed view of this value as an array (for iteration, size(), and Push()).
  ///
  /// @return  Returns the view. A non-array value or an invalid JSON returns an empty view.
  ///
  /// @note  A const JSON doesn't protect an array from appends-- Push() still works through the
  ///        view.
  ///
  JSONArray AsArray() const;

  ///
  /// Serialize this value to JSON text.
  ///
  /// @param  indent  The number of spaces per nesting level. Pass 0 for compact single-line
  ///                 output.
  ///
  /// @return  Returns the JSON text, or "" when this value is invalid.
  ///
  String Stringify(unsigned indent = 0) const;

  ///
  /// Deep-copy this value into an independent document.
  ///
  /// Plain copies share the document (see the class overview).
  ///
  /// @return  Returns the copy, or an invalid JSON if this JSON is invalid.
  ///
  JSON Clone() const;

 private:
  friend class JSONObject;
  friend class JSONArray;
  explicit JSON(detail::JSONData* adopted_node);

  detail::JSONData* node_ = nullptr;
};

///
/// One object member, as visited by JSONObject iteration.
///
struct JSON::Member {
  String key;  ///< The member name.
  JSON value;  ///< The member's value (shared with the document).
};

///
/// Assignable reference to a value inside a mutable JSON document.
///
/// Indexing a non-const JSON document with JSON::operator[]() produces a JSON::Ref. It acts as an
/// assignable reference to a member or element, letting you modify data or chain deeper lookups.
///
/// Assigning through a JSON::Ref writes directly into the underlying document. When indexing
/// through keys that don't exist yet, the reference creates intermediate objects along the path
/// automatically.
///
/// You can store a reference to a nested object to update several values in the same branch:
///
/// ```
/// JSON save = JSON::Object();
///
/// auto audio = save["audio"];  // a JSON::Ref into save
/// audio["volume"] = 0.8;       // writes save["audio"]["volume"]
/// audio["muted"] = false;
///
/// JSON audio_values = save["audio"];  // a JSON, for AsObject() and AsArray()
/// size_t count = audio_values.AsObject().size();
/// ```
///
/// ## Storing a Reference
///
/// The variable type you choose when holding a lookup determines how assignment behaves and which
/// methods are available:
///
/// - **Assigning to a stored JSON::Ref doesn't rebind the variable.** It still writes into the
///   document.
/// - **Assigning to a JSON variable rebinds the variable.** Reassigning a JSON instance leaves the
///   document untouched, and the type provides AsObject() and AsArray() to inspect collections.
///
/// @see  JSON, JSON::operator[]()
///
class JSON::Ref {
 public:
  ///
  /// Copy constructor (copies the reference itself, not the referenced value).
  ///
  UExport Ref(const Ref& other);

  ///
  /// Destructor.
  ///
  UExport ~Ref();

  ///
  /// Chain to an object member, creating the referenced place as an object if it is missing or
  /// holds null.
  ///
  /// @param  key  The member name.
  ///
  /// @return  Returns a reference to the member.
  ///
  /// @note  If the referenced place holds another kind of value, writes through the result do
  ///        nothing.
  ///
  UExport Ref operator[](const String& key);

  ///
  /// Chain to an array element.
  ///
  /// @param  index  The element index.
  ///
  /// @return  Returns a reference to the element.
  ///
  /// @note  The referenced place must already hold an array. Otherwise writes through the result
  ///        do nothing.
  ///
  UExport Ref operator[](size_t index);

  ///
  /// Write `value` into the referenced place, sharing its document.
  ///
  /// @note  An invalid `value` is ignored.
  ///
  UExport Ref& operator=(const JSON& value);

  ///
  /// Write the value `other` refers to into the referenced place.
  ///
  /// @note  This assigns the value. This Ref continues to refer to its existing place.
  ///
  UExport Ref& operator=(const Ref& other);

  ///
  /// Write a boolean into the referenced place.
  ///
  UExport Ref& operator=(bool value);

  ///
  /// Write a number into the referenced place.
  ///
  UExport Ref& operator=(double value);

  ///
  /// Write a number into the referenced place.
  ///
  UExport Ref& operator=(int value);

  ///
  /// Write a number into the referenced place.
  ///
  UExport Ref& operator=(unsigned value);

  ///
  /// Write a number into the referenced place.
  ///
  UExport Ref& operator=(long value);

  ///
  /// Write a number into the referenced place.
  ///
  UExport Ref& operator=(unsigned long value);

  ///
  /// Write a number into the referenced place.
  ///
  UExport Ref& operator=(long long value);

  ///
  /// Write a number into the referenced place.
  ///
  UExport Ref& operator=(unsigned long long value);

  ///
  /// Write a string into the referenced place.
  ///
  UExport Ref& operator=(const String& value);

  ///
  /// Write a string into the referenced place.
  ///
  UExport Ref& operator=(const char* value);

  ///
  /// Write a JSON null into the referenced place.
  ///
  UExport Ref& operator=(decltype(nullptr));

  ///
  /// Convert to a JSON sharing the referenced value.
  ///
  /// @return  Returns the value, or an invalid JSON if the referenced place is missing.
  ///
  UExport operator JSON() const;

  ///
  /// Whether or not the referenced place holds a value.
  ///
  UExport bool is_valid() const;

  ///
  /// Whether or not the referenced place holds a value (the same as is_valid()).
  ///
  explicit operator bool() const { return is_valid(); }

  ///
  /// Whether or not the referenced place holds a JSON null.
  ///
  /// This pairs with assigning `nullptr`.
  ///
  /// @note  This returns false when the referenced place is missing.
  ///
  UExport bool is_null() const;

  ///
  /// Read the referenced value as a number (the same as JSON::Or()).
  ///
  /// @param  fallback  The value to return when the referenced value is not a number.
  ///
  /// @return  Returns the number or `fallback`.
  ///
  UExport double Or(double fallback) const;

  ///
  /// Read the referenced value as a number, truncated toward zero to an integer.
  ///
  /// @param  fallback  The value to return when the referenced value is not a number.
  ///
  /// @return  Returns the truncated number or `fallback`. Numbers beyond the 64-bit range
  ///          clamp to the nearest representable integer. NaN reads as 0.
  ///
  UExport int64_t Or(long long fallback) const;

  ///
  /// Same as Or(long long).
  ///
  UExport int64_t Or(int fallback) const;

  ///
  /// Same as Or(long long).
  ///
  UExport int64_t Or(unsigned fallback) const;

  ///
  /// Same as Or(long long).
  ///
  UExport int64_t Or(long fallback) const;

  ///
  /// Same as Or(long long).
  ///
  UExport int64_t Or(unsigned long fallback) const;

  ///
  /// Same as Or(long long).
  ///
  UExport int64_t Or(unsigned long long fallback) const;

  ///
  /// Same as Or(double).
  ///
  UExport double Or(float fallback) const;

  ///
  /// Read the referenced value as a boolean.
  ///
  /// @param  fallback  The value to return when the referenced value is not a boolean.
  ///
  /// @return  Returns the boolean or `fallback`.
  ///
  UExport bool Or(bool fallback) const;

  ///
  /// Read the referenced value as a string.
  ///
  /// @param  fallback  The value to return when the referenced value is not a string.
  ///
  /// @return  Returns the string or `fallback`.
  ///
  UExport String Or(const String& fallback) const;

  ///
  /// Same as Or(const String&).
  ///
  UExport String Or(const char* fallback) const;

 private:
  friend class JSON;
  Ref(detail::JSONData* parent, const String& key);
  Ref(detail::JSONData* parent, size_t index);

  detail::JSONData* parent_;
  String key_;
  size_t index_;
  bool keyed_;
};

///
/// Typed view of a JSON object for checking keys and stepping through its members.
///
/// You obtain a JSONObject by calling JSON::AsObject() on a JSON value.
///
/// This function checks for a key and iterates over every member in an object:
///
/// ```
/// void ConnectServers(const JSON& prefs) {
///   JSONObject servers = prefs["servers"].AsObject();
///   if (!servers.Has("default"))
///     Log("no default server");
///
///   for (JSON::Member member : servers)
///     Connect(member.key, member.value.Or(""));
/// }
/// ```
///
/// ## Member Iteration
///
/// Iteration visits members in the order they were parsed or first added, and assigning to an
/// existing key keeps its position.
///
/// Calling JSON::AsObject() on a missing key or a value that isn't an object produces an empty
/// view, so the loop doesn't run.
///
/// @see  JSON::AsObject(), JSON::Member, JSONArray
///
class UExport JSONObject {
 public:
  ///
  /// Input iterator over an object's members (yields JSON::Member by value).
  ///
  class iterator {
   public:
    JSON::Member operator*() const { return object_->MemberAt(index_); }
    iterator& operator++() {
      index_++;
      return *this;
    }
    bool operator==(const iterator& other) const { return index_ == other.index_; }
    bool operator!=(const iterator& other) const { return index_ != other.index_; }

   private:
    friend class JSONObject;
    iterator(const JSONObject* object, size_t index) : object_(object), index_(index) {}
    const JSONObject* object_;
    size_t index_;
  };

  ///
  /// Copy constructor.
  ///
  JSONObject(const JSONObject& other);

  ///
  /// Destructor.
  ///
  ~JSONObject();

  ///
  /// Get the number of members (0 for an empty view).
  ///
  size_t size() const;

  ///
  /// Whether or not the object has a member with the given key.
  ///
  bool Has(const String& key) const;

  ///
  /// Get an iterator to the first member (insertion order).
  ///
  iterator begin() const { return iterator(this, 0); }

  ///
  /// Get an iterator past the last member.
  ///
  iterator end() const { return iterator(this, size()); }

 private:
  friend class JSON;
  explicit JSONObject(detail::JSONData* node);
  JSONObject& operator=(const JSONObject&) = delete;
  JSON::Member MemberAt(size_t index) const;

  detail::JSONData* node_;
};

///
/// A view of a JSON array.
///
/// Calling JSON::AsArray() on a JSON value returns a JSONArray view. You use it to step through
/// array elements in a range-for loop and append new values.
///
/// This example appends values to an array and logs each element:
///
/// ```
/// JSON scores = JSON::Array();
/// JSONArray entries = scores.AsArray();
/// entries.Push(1200);
/// entries.Push(950);
///
/// for (JSON score : entries)
///   Log(score.Stringify().utf8().data());
/// ```
///
/// ## Elements and Appending
///
/// Iterating over the array yields each element as a JSON that shares the underlying document.
///
/// Calling JSON::AsArray() on a value that isn't an array returns an empty view. The loop doesn't
/// run, and Push() does nothing.
///
/// @see  JSON::AsArray(), JSONObject
///
class UExport JSONArray {
 public:
  ///
  /// Input iterator over an array's elements (yields JSON by value).
  ///
  class iterator {
   public:
    JSON operator*() const { return array_->ElementAt(index_); }
    iterator& operator++() {
      index_++;
      return *this;
    }
    bool operator==(const iterator& other) const { return index_ == other.index_; }
    bool operator!=(const iterator& other) const { return index_ != other.index_; }

   private:
    friend class JSONArray;
    iterator(const JSONArray* array, size_t index) : array_(array), index_(index) {}
    const JSONArray* array_;
    size_t index_;
  };

  ///
  /// Copy constructor.
  ///
  JSONArray(const JSONArray& other);

  ///
  /// Destructor.
  ///
  ~JSONArray();

  ///
  /// Get the number of elements (0 for an empty view).
  ///
  size_t size() const;

  ///
  /// Append `value`, sharing its document.
  ///
  /// @note  An invalid `value` appends nothing.
  ///
  void Push(const JSON& value);

  ///
  /// Append a boolean.
  ///
  void Push(bool value);

  ///
  /// Append a number.
  ///
  void Push(double value);

  ///
  /// Append a number.
  ///
  void Push(int value);

  ///
  /// Append a number.
  ///
  void Push(unsigned value);

  ///
  /// Append a number.
  ///
  void Push(long value);

  ///
  /// Append a number.
  ///
  void Push(unsigned long value);

  ///
  /// Append a number.
  ///
  void Push(long long value);

  ///
  /// Append a number.
  ///
  void Push(unsigned long long value);

  ///
  /// Append a string.
  ///
  void Push(const String& value);

  ///
  /// Append a string.
  ///
  void Push(const char* value);

  ///
  /// Append a JSON null.
  ///
  void Push(decltype(nullptr));

  ///
  /// Get an iterator to the first element.
  ///
  iterator begin() const { return iterator(this, 0); }

  ///
  /// Get an iterator past the last element.
  ///
  iterator end() const { return iterator(this, size()); }

 private:
  friend class JSON;
  explicit JSONArray(detail::JSONData* node);
  JSONArray& operator=(const JSONArray&) = delete;
  JSON ElementAt(size_t index) const;

  detail::JSONData* node_;
};

}  // namespace ultralight
