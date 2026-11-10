///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None
#include <Ultralight/Defines.h>
#include <Ultralight/String.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/Color.h>

namespace ultralight {

///
/// The editing commands you can run with Editor::Execute().
///
/// Most commands act on editable text (a text field or an editable region). Copy, SelectAll, and
/// the selection commands (the AndModifySelection forms, SelectWord, SelectLine, SelectParagraph,
/// and Unselect) also work on a selection in regular page content.
///
/// Every caret motion has an AndModifySelection form that extends the selection instead of moving
/// the caret (the Shift-key behavior).
///
enum class EditorCommand : uint8_t {
  // Clipboard, selection, and history commands (the Edit-menu set).
  Cut = 0,           ///< Cut the selection to the clipboard.
  Copy,              ///< Copy the selection to the clipboard.
  Paste,             ///< Paste the clipboard's contents.
  PasteAsPlainText,  ///< Paste the clipboard's text without its formatting.
  SelectAll,         ///< Select all the text (or the whole page).
  Undo,              ///< Undo the last edit.
  Redo,              ///< Redo the last undone edit.

  // Caret motions.
  MoveLeft,                    ///< Move the caret one character left.
  MoveRight,                   ///< Move the caret one character right.
  MoveUp,                      ///< Move the caret one line up.
  MoveDown,                    ///< Move the caret one line down.
  MoveWordLeft,                ///< Move the caret one word left.
  MoveWordRight,               ///< Move the caret one word right.
  MoveToBeginningOfLine,       ///< Move the caret to the start of the line.
  MoveToEndOfLine,             ///< Move the caret to the end of the line.
  MoveToBeginningOfParagraph,  ///< Move the caret to the start of the paragraph.
  MoveToEndOfParagraph,        ///< Move the caret to the end of the paragraph.
  MoveToBeginningOfDocument,   ///< Move the caret to the start of the text.
  MoveToEndOfDocument,         ///< Move the caret to the end of the text.
  MovePageUp,                  ///< Move the caret one page up.
  MovePageDown,                ///< Move the caret one page down.

  // Selection-extending forms of each caret motion.
  MoveLeftAndModifySelection,                    ///< Select one character left.
  MoveRightAndModifySelection,                   ///< Select one character right.
  MoveUpAndModifySelection,                      ///< Select one line up.
  MoveDownAndModifySelection,                    ///< Select one line down.
  MoveWordLeftAndModifySelection,                ///< Select one word left.
  MoveWordRightAndModifySelection,               ///< Select one word right.
  MoveToBeginningOfLineAndModifySelection,       ///< Select to line start.
  MoveToEndOfLineAndModifySelection,             ///< Select to line end.
  MoveToBeginningOfParagraphAndModifySelection,  ///< Select to paragraph start.
  MoveToEndOfParagraphAndModifySelection,        ///< Select to paragraph end.
  MoveToBeginningOfDocumentAndModifySelection,   ///< Select to text start.
  MoveToEndOfDocumentAndModifySelection,         ///< Select to text end.
  MovePageUpAndModifySelection,                  ///< Select one page up.
  MovePageDownAndModifySelection,                ///< Select one page down.

  // Selection commands.
  SelectWord,       ///< Select the word at the caret.
  SelectLine,       ///< Select the line at the caret.
  SelectParagraph,  ///< Select the paragraph at the caret.
  Unselect,         ///< Clear the selection (no caret remains).

  // Deletion commands.
  DeleteBackward,           ///< Delete the selection or the previous character.
  DeleteForward,            ///< Delete the selection or the next character.
  DeleteWordBackward,       ///< Delete the word before the caret.
  DeleteWordForward,        ///< Delete the word after the caret.
  DeleteToBeginningOfLine,  ///< Delete to the start of the line.
  DeleteToEndOfLine,        ///< Delete to the end of the line.

  // Insertion commands.
  InsertNewline,    ///< Insert a new paragraph (the Enter key).
  InsertLineBreak,  ///< Insert a line break in the paragraph (Shift+Enter).
  InsertTab,        ///< Insert a tab.
  InsertBacktab,    ///< Insert a backtab (Shift+Tab).
};

///
/// The kind of element that holds keyboard focus (see EditableState).
///
enum class EditableType : uint8_t {
  None = 0,         ///< No element holds keyboard focus.
  Text,             ///< A single-line text field.
  Password,         ///< A password field.
  TextArea,         ///< A multi-line text area.
  ContentEditable,  ///< An editable (contenteditable) region.
  Other,            ///< A focused element that is not a text-editing target (eg, a select).
};

///
/// The focused element's inputmode hint (what kind of virtual keyboard the page asks for).
///
/// @see  <https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/inputmode>
///
enum class InputMode : uint8_t {
  Unspecified = 0,
  None,
  Text,
  Telephone,
  Url,
  Email,
  Numeric,
  Decimal,
  Search,
};

///
/// The focused element's enterkeyhint (what label or icon the Enter key should present).
///
/// @see  <https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/enterkeyhint>
///
enum class EnterKeyHint : uint8_t {
  Unspecified = 0,
  Enter,
  Done,
  Go,
  Next,
  Previous,
  Search,
  Send,
};

///
/// The conditions that decide whether an input method should be active.
///
/// Reported through EditorListener::OnChangeEditableState(). An input method or on-screen keyboard
/// should be active when both `view_has_focus` and `is_mutable` are true.
///
/// @note  This is reported when focus moves, not when the focused element moves or resizes. To
///        follow the caret, use Editor::GetCaretBounds().
///
/// @see  EditorListener::OnChangeEditableState()
///
struct UExport EditableState {
  ///
  /// The kind of element that holds keyboard focus.
  ///
  EditableType type = EditableType::None;

  ///
  /// Whether or not the focused element accepts typed text (false for a read-only or disabled text
  /// control, and always false when `type` is None or Other).
  ///
  bool is_mutable = false;

  ///
  /// Whether or not the View itself has input focus (see View::Focus() and View::Unfocus()).
  ///
  bool view_has_focus = false;

  ///
  /// The focused element's inputmode hint.
  ///
  InputMode input_mode = InputMode::Unspecified;

  ///
  /// The focused element's enterkeyhint.
  ///
  EnterKeyHint enter_key_hint = EnterKeyHint::Unspecified;

  ///
  /// The focused element's bounds in viewport CSS pixels (the same space as
  /// View::FireMouseEvent()). All-zero when no element has focus.
  ///
  Rect bounds = { 0.0f, 0.0f, 0.0f, 0.0f };
};

///
/// A range of characters within a composition's text (see CompositionSegment and
/// Editor::SetComposition()).
///
struct UExport CompositionRange {
  ///
  /// The starting offset of the range, in UTF-16 code units.
  ///
  uint32_t start = 0;

  ///
  /// The ending offset of the range (exclusive), in UTF-16 code units.
  ///
  uint32_t end = 0;
};

///
/// Styling for one segment of a composition's text (see Editor::SetComposition()).
///
/// An OS input method usually reports one segment per clause of the in-progress text, with
/// the clause the user is actively converting marked `thick`.
///
struct UExport CompositionSegment {
  ///
  /// The range of composition text this segment styles.
  ///
  CompositionRange range;

  ///
  /// The underline color. Leave unset to draw the underline in the text's own color.
  ///
  Color underline_color;

  ///
  /// A highlight fill drawn behind the segment's text. Leave unset for no fill.
  ///
  Color background_color;

  ///
  /// Whether the underline is drawn thick (the convention for the active clause).
  ///
  bool thick = false;
};

///
/// Text-editing interface for a View.
///
/// The editor executes editing commands and manages composition state for the focused element in a
/// View.
///
/// Standard keyboard shortcuts like Ctrl+C and Ctrl+V run automatically when you forward key events
/// to a View. You use the editor to connect native UI controls like Edit menus and on-screen
/// keyboards directly to the page, and to relay text from an OS input method.
///
/// This example updates native Edit menu items before display and runs the command the user picks:
///
/// ```
/// void UpdateEditMenu() {
///   Editor* editor = view->editor();
///   SetMenuItemEnabled("Cut", editor->CanExecute(EditorCommand::Cut));
///   SetMenuItemEnabled("Copy", editor->CanExecute(EditorCommand::Copy));
///   SetMenuItemEnabled("Paste", editor->CanExecute(EditorCommand::Paste));
/// }
///
/// void OnEditMenuCommand(EditorCommand command) {
///   view->editor()->Execute(command);
/// }
/// ```
///
/// ## Running Commands
///
/// Call View::editor() to obtain the View's Editor instance. Commands target the focused element in
/// the active frame.
///
/// Calling Execute() runs an EditorCommand on that element. It returns `false` when the command
/// can't run, such as copying without an active selection or undoing with an empty history.
///
/// Call CanExecute() to test whether a command would succeed right now, which lets you enable or
/// disable native menu items before presenting them.
///
/// @note  ViewConfig::clipboard_read_policy restricts clipboard reads attempted by page scripts,
///        but it doesn't restrict pastes executed through Execute().
///
/// ## Inserting Text
///
/// Call InsertText() to insert text at the caret, such as when forwarding input from an on-screen
/// keyboard. The text replaces any active selection, exactly as if the user typed it.
///
/// This callback forwards tapped keys to the focused element:
///
/// ```
/// void OnScreenKeyTapped(const String& key_text) {
///   view->editor()->InsertText(key_text);
/// }
/// ```
///
/// ## Supporting Input Methods
///
/// AppCore windows run the OS input method automatically for their panels. When you host a View
/// directly, you implement EditorListener and pass it to View::set_editor_listener() to coordinate
/// the input method with the page.
///
/// ### Activating the Input Method
///
/// In EditorListener::OnChangeEditableState(), turn on the OS input method only while both
/// `view_has_focus` and `is_mutable` are `true` in EditableState (turn it off whenever either
/// property is `false`).
///
/// ### Composing Text
///
/// Pass in-progress composition text from the OS to SetComposition(). The initial call starts the
/// composition and replaces any active selection on the page-- later calls replace the previous
/// in-progress text.
///
/// ### Placing the Candidate Window
///
/// Implement EditorListener::OnUpdateComposition() to reposition the candidate window whenever the
/// composition updates.
///
/// Query GetCompositionCharacterBounds() at index 0 to anchor the candidate window to the
/// composition's first character, or GetCaretBounds() when no composition is active. Anchoring to
/// the first character prevents the candidate window from jumping as candidate words change length.
///
/// Both methods return rectangles in viewport CSS pixels. Multiply their coordinates by
/// View::device_scale() to convert them to device pixels.
///
/// ### Ending a Composition
///
/// Call the matching editor method when the input method completes or cancels:
///
/// - **CommitComposition() applies final text.** If no composition is active, it inserts the text
///   at the caret.
/// - **FinishComposition() keeps the in-progress text as final text.** Call this when the input
///   method closes without committing (such as when focus moves away mid-composition).
/// - **CancelComposition() removes the in-progress text.** Call this when the input method cancels
///   without producing text.
///
/// When the page terminates an active composition on its own (such as during navigation or when a
/// click focuses a different element), the library calls EditorListener::OnDiscardComposition().
/// The page keeps the in-progress text as final text, so cancel the OS input method without
/// recommitting or inserting the text again.
///
/// ### Inspecting Surrounding Text
///
/// OS input methods sometimes inspect or replace text surrounding the caret, such as when showing
/// an accent palette.
///
/// Call GetSelectionOffsets() to locate the selection. The editor's offset methods measure
/// character positions in UTF-16 code units from the start of the focused editable element,
/// matching the DOM `selectionStart` and `selectionEnd` properties.
///
/// @see  View::editor(), EditorListener, EditableState, EditorCommand
///
class UExport Editor {
 public:
  ///
  /// Run an editing command.
  ///
  /// @param  command  The command to run.
  ///
  /// @return  Returns whether the command ran (false whenever CanExecute() would return false).
  ///
  virtual bool Execute(EditorCommand command) = 0;

  ///
  /// Whether or not a command would do anything right now.
  ///
  /// You can use this to grey out items in a native menu.
  ///
  /// @param  command  The command to check.
  ///
  /// @return  Returns true if Execute() would run the command.
  ///
  [[nodiscard]] virtual bool CanExecute(EditorCommand command) const = 0;

  ///
  /// Insert text at the caret, replacing the selection (the same as typing it).
  ///
  /// @param  text  The text to insert.
  ///
  /// @return  Returns whether the text was inserted (false when there's no editable text to insert
  ///          into).
  ///
  virtual bool InsertText(const String& text) = 0;

  ///
  /// Get the bounds of the caret.
  ///
  /// The rect is in viewport CSS pixels (the same space as View::FireMouseEvent()). Multiply by
  /// View::device_scale() for device pixels.
  ///
  /// @return  Returns the caret bounds (an empty rect when there's no caret in editable text).
  ///
  /// @note  This updates any pending layout first-- query it when you need it (eg, when opening a
  ///        menu) rather than every frame.
  ///
  [[nodiscard]] virtual Rect GetCaretBounds() = 0;

  ///
  /// Set or update the in-progress text of an input method's composition.
  ///
  /// The first call starts a composition and replaces the selection. Later calls replace the
  /// previous in-progress text.
  ///
  /// @param  text          The full in-progress text. Pass an empty string to remove it (the same
  ///                       as CancelComposition()).
  ///
  /// @param  segments      Underline styles for parts of the text (eg, one per clause). Pass a
  ///                       nullptr to underline all of it.
  ///
  /// @param  num_segments  The number of entries in `segments`.
  ///
  /// @param  selection     The caret or selection within `text`, in UTF-16 code units.
  ///
  /// @return  Returns whether the composition was applied (false when there's no editable text).
  ///
  /// @note  During a composition, the page gets the standard composition events. Every keydown
  ///        reports key code 229 (the same as in a browser).
  ///
  virtual bool SetComposition(const String& text, const CompositionSegment* segments,
                              size_t num_segments, const CompositionRange& selection) = 0;

  ///
  /// Replace the active composition with the input method's final text.
  ///
  /// With no active composition, this inserts `text` at the caret.
  ///
  /// @param  text  The final text.
  ///
  /// @return  Returns whether the text was inserted (false when there's no editable text).
  ///
  virtual bool CommitComposition(const String& text) = 0;

  ///
  /// Keep the active composition's in-progress text as final text.
  ///
  /// Call this when the input method closes without committing or cancelling (eg, focus moves away
  /// mid-composition).
  ///
  /// @return  Returns whether a composition was finished (false when none was active).
  ///
  virtual bool FinishComposition() = 0;

  ///
  /// Cancel the active composition and remove its text.
  ///
  /// @return  Returns whether a composition was cancelled (false when none was active).
  ///
  virtual bool CancelComposition() = 0;

  ///
  /// Whether or not a composition is active.
  ///
  [[nodiscard]] virtual bool HasComposition() const = 0;

  ///
  /// Get the bounds of the active composition's text, in viewport CSS pixels.
  ///
  /// You can use this as the area a candidate window shouldn't cover.
  ///
  /// @return  Returns the bounds (all-zero when no composition is active).
  ///
  [[nodiscard]] virtual Rect GetCompositionBounds() = 0;

  ///
  /// Get the length of the active composition's text, in UTF-16 code units (0 when none is active).
  ///
  [[nodiscard]] virtual uint32_t GetCompositionCharacterCount() = 0;

  ///
  /// Get the bounds of one character of the active composition's text, in viewport CSS pixels.
  ///
  /// You can use this to place a candidate window. Anchor it on the composition's first character
  /// (index 0) so it doesn't move as the candidates change.
  ///
  /// @param  index  The character's offset within the composition's text, in UTF-16 code units.
  ///
  /// @return  Returns the character's bounds (all-zero when `index` is out of range or no
  ///          composition is active).
  ///
  [[nodiscard]] virtual Rect GetCompositionCharacterBounds(uint32_t index) = 0;

  ///
  /// Get the selection's offsets within the focused element's text.
  ///
  /// The offset methods (this, GetTextInRange(), ReplaceTextInRange(), and
  /// GetCompositionOffsets()) use UTF-16 code units into the text of the focused text field or
  /// editable region. For `<input>` and `<textarea>`, they match the element's `selectionStart` and
  /// `selectionEnd`.
  ///
  /// @param  start  Set to the selection's start.
  ///
  /// @param  end    Set to the selection's end (the same as `start` for a caret).
  ///
  /// @return  Returns whether the offsets were set (false when the focused element isn't editable
  ///          text, or the selection is outside it).
  ///
  [[nodiscard]] virtual bool GetSelectionOffsets(uint32_t& start, uint32_t& end) = 0;

  ///
  /// Get part of the focused element's text (see GetSelectionOffsets() for the offsets).
  ///
  /// Offsets past the end of the text are clamped to it.
  ///
  /// @param  start  The start offset.
  ///
  /// @param  end    The end offset (exclusive).
  ///
  /// @return  Returns the text in the range (empty when the focused element isn't editable text).
  ///
  /// @warning  This reads password fields too, so treat the result as sensitive.
  ///
  [[nodiscard]] virtual String GetTextInRange(uint32_t start, uint32_t end) = 0;

  ///
  /// Replace part of the focused element's text (see GetSelectionOffsets() for the offsets).
  ///
  /// This edits the same way typing does. The edit is one undo step, and the caret ends up after
  /// the new text. You can use this for an input method's replacement ranges (eg, the macOS accent
  /// picker replaces the character it targets).
  ///
  /// Offsets past the end of the text are clamped to it.
  ///
  /// @param  start  The start offset.
  ///
  /// @param  end    The end offset (exclusive).
  ///
  /// @param  text   The new text (an empty string deletes the range).
  ///
  /// @return  Returns whether the text was replaced (false when the focused element isn't editable
  ///          text, or `start` is greater than `end`).
  ///
  /// @note  An active composition is cancelled first, and the offsets apply to the text left after
  ///        that.
  ///
  virtual bool ReplaceTextInRange(uint32_t start, uint32_t end, const String& text) = 0;

  ///
  /// Get the active composition's offsets within the focused element's text (see
  /// GetSelectionOffsets() for the offsets).
  ///
  /// You can use this to report the input method's marked range.
  ///
  /// @param  start  Set to the composition's start.
  ///
  /// @param  end    Set to the composition's end (exclusive).
  ///
  /// @return  Returns whether the offsets were set (false when no composition is active in the
  ///          focused element).
  ///
  [[nodiscard]] virtual bool GetCompositionOffsets(uint32_t& start, uint32_t& end) = 0;

 protected:
  virtual ~Editor() = default;
};

}  // namespace ultralight

#pragma pop_macro("None")
