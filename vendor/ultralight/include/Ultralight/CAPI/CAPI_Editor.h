///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_Editor.h
///
/// Text-editing interface for a View.
///
/// `#include <Ultralight/CAPI/CAPI_Editor.h>`
///
/// The C editor functions run editing commands and manage composition state for the focused element
/// in a View.
///
/// Standard keyboard shortcuts like Ctrl+C and Ctrl+V run automatically when you dispatch key
/// events to a View. You use the editor functions to connect native controls like Edit menus and
/// on-screen keyboards directly to the page, or to relay text from an OS input method.
///
/// This example updates native Edit menu items before display and runs the command the user picks:
///
/// ```c
/// static void UpdateEditMenu(void) {
///   ULEditor editor = ulViewGetEditor(view);
///   SetMenuItemEnabled("Cut", ulEditorCanExecute(editor, kEditorCommand_Cut));
///   SetMenuItemEnabled("Copy", ulEditorCanExecute(editor, kEditorCommand_Copy));
///   SetMenuItemEnabled("Paste", ulEditorCanExecute(editor, kEditorCommand_Paste));
/// }
///
/// static void OnEditMenuCommand(ULEditorCommand command) {
///   ulEditorExecute(ulViewGetEditor(view), command);
/// }
/// ```
///
/// ## The Editor Handle
///
/// Call ulViewGetEditor() with a ULView handle to obtain its editor. The returned ULEditor is
/// borrowed (it's never `NULL`), so you don't destroy it yourself. It belongs to the handle you
/// passed and stays valid until you destroy that handle with ulDestroyView().
///
/// ## Input Methods on Hosted Views
///
/// When you host a View directly, register the editing callbacks in `<Ultralight/CAPI/CAPI_View.h>`
/// to coordinate an OS input method with the page:
///
/// - **ulViewSetChangeEditableStateCallback() reports when the focused element accepts text.** Turn
///   the input method on only while both `view_has_focus` and `is_mutable` are `true` in
///   ULEditableState.
/// - **ulViewSetUpdateCompositionCallback() signals when to reposition the candidate window.**
///   Anchor the candidate window to the composition's first character with
///   ulEditorGetCompositionCharacterBounds().
/// - **ulViewSetDiscardCompositionCallback() signals when the page drops the active composition.**
///   Cancel the OS input method without recommitting or inserting the text again.
///
/// For the complete input method lifecycle and candidate window placement rules, see
/// `<Ultralight/Editor.h>`.
///
/// When styling composition clauses for ulEditorSetComposition(), zero-initialize each
/// ULCompositionSegment struct. A zero-initialized ULColor represents an unset color.
///
/// This function updates the in-progress composition text and styles the active clause with a thick
/// underline:
///
/// ```c
/// // Offsets are in UTF-16 code units.
/// static void OnCompositionUpdate(ULString text, unsigned int text_length,
///                                 unsigned int caret) {
///   ULCompositionSegment clause = {0};  // colors left unset
///   clause.range.end = text_length;
///   clause.thick = true;
///
///   ULCompositionRange selection = {caret, caret};
///   ulEditorSetComposition(ulViewGetEditor(view), text, &clause, 1, selection);
/// }
/// ```
///
/// @note  AppCore panel Views already set the editing callbacks to run the window's input method.
///        Setting an editing callback on a panel's View replaces AppCore's callback and turns off
///        input method support for that View.
///
/// @see  `<Ultralight/Editor.h>`, ulViewGetEditor(), ulViewSetChangeEditableStateCallback(),
///       ULEditableState, ULEditorCommand
///

#ifndef ULTRALIGHT_CAPI_EDITOR_H
#define ULTRALIGHT_CAPI_EDITOR_H

#include <Ultralight/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_Geometry.h>
#include <Ultralight/CAPI/CAPI_Color.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Editor
 *****************************************************************************/

///
/// The editing commands you can run with ulEditorExecute().
///
/// Most commands act on editable text (a text field or an editable region). Copy, SelectAll, and
/// the selection commands (the AndModifySelection forms, SelectWord, SelectLine, SelectParagraph,
/// and Unselect) also work on a selection in regular page content.
///
/// Every caret motion has an AndModifySelection form that extends the selection instead of moving
/// the caret (the Shift-key behavior).
///
typedef enum {
  // Clipboard, selection, and history commands (the Edit-menu set).
  kEditorCommand_Cut = 0,           ///< Cut the selection to the clipboard.
  kEditorCommand_Copy,              ///< Copy the selection to the clipboard.
  kEditorCommand_Paste,             ///< Paste the clipboard's contents.
  kEditorCommand_PasteAsPlainText,  ///< Paste the clipboard's text without its formatting.
  kEditorCommand_SelectAll,         ///< Select all the text (or the whole page).
  kEditorCommand_Undo,              ///< Undo the last edit.
  kEditorCommand_Redo,              ///< Redo the last undone edit.

  // Caret motions.
  kEditorCommand_MoveLeft,                    ///< Move the caret one character left.
  kEditorCommand_MoveRight,                   ///< Move the caret one character right.
  kEditorCommand_MoveUp,                      ///< Move the caret one line up.
  kEditorCommand_MoveDown,                    ///< Move the caret one line down.
  kEditorCommand_MoveWordLeft,                ///< Move the caret one word left.
  kEditorCommand_MoveWordRight,               ///< Move the caret one word right.
  kEditorCommand_MoveToBeginningOfLine,       ///< Move the caret to the start of the line.
  kEditorCommand_MoveToEndOfLine,             ///< Move the caret to the end of the line.
  kEditorCommand_MoveToBeginningOfParagraph,  ///< Move the caret to the start of the paragraph.
  kEditorCommand_MoveToEndOfParagraph,        ///< Move the caret to the end of the paragraph.
  kEditorCommand_MoveToBeginningOfDocument,   ///< Move the caret to the start of the text.
  kEditorCommand_MoveToEndOfDocument,         ///< Move the caret to the end of the text.
  kEditorCommand_MovePageUp,                  ///< Move the caret one page up.
  kEditorCommand_MovePageDown,                ///< Move the caret one page down.

  // Selection-extending forms of each caret motion.
  kEditorCommand_MoveLeftAndModifySelection,                    ///< Select one character left.
  kEditorCommand_MoveRightAndModifySelection,                   ///< Select one character right.
  kEditorCommand_MoveUpAndModifySelection,                      ///< Select one line up.
  kEditorCommand_MoveDownAndModifySelection,                    ///< Select one line down.
  kEditorCommand_MoveWordLeftAndModifySelection,                ///< Select one word left.
  kEditorCommand_MoveWordRightAndModifySelection,               ///< Select one word right.
  kEditorCommand_MoveToBeginningOfLineAndModifySelection,       ///< Select to line start.
  kEditorCommand_MoveToEndOfLineAndModifySelection,             ///< Select to line end.
  kEditorCommand_MoveToBeginningOfParagraphAndModifySelection,  ///< Select to paragraph start.
  kEditorCommand_MoveToEndOfParagraphAndModifySelection,        ///< Select to paragraph end.
  kEditorCommand_MoveToBeginningOfDocumentAndModifySelection,   ///< Select to text start.
  kEditorCommand_MoveToEndOfDocumentAndModifySelection,         ///< Select to text end.
  kEditorCommand_MovePageUpAndModifySelection,                  ///< Select one page up.
  kEditorCommand_MovePageDownAndModifySelection,                ///< Select one page down.

  // Selection commands.
  kEditorCommand_SelectWord,       ///< Select the word at the caret.
  kEditorCommand_SelectLine,       ///< Select the line at the caret.
  kEditorCommand_SelectParagraph,  ///< Select the paragraph at the caret.
  kEditorCommand_Unselect,         ///< Clear the selection (no caret remains).

  // Deletion commands.
  kEditorCommand_DeleteBackward,           ///< Delete the selection or the previous character.
  kEditorCommand_DeleteForward,            ///< Delete the selection or the next character.
  kEditorCommand_DeleteWordBackward,       ///< Delete the word before the caret.
  kEditorCommand_DeleteWordForward,        ///< Delete the word after the caret.
  kEditorCommand_DeleteToBeginningOfLine,  ///< Delete to the start of the line.
  kEditorCommand_DeleteToEndOfLine,        ///< Delete to the end of the line.

  // Insertion commands.
  kEditorCommand_InsertNewline,    ///< Insert a new paragraph (the Enter key).
  kEditorCommand_InsertLineBreak,  ///< Insert a line break in the paragraph (Shift+Enter).
  kEditorCommand_InsertTab,        ///< Insert a tab.
  kEditorCommand_InsertBacktab,    ///< Insert a backtab (Shift+Tab).
} ULEditorCommand;

///
/// Get the editor for a View.
///
/// @param  view  The View.
///
/// @return  Returns the View's editor (this is never NULL).
///
/// @note  Don't destroy the returned handle, it is owned by `view` and stays valid until you call
///        ulDestroyView() on it.
///
ULExport ULEditor ulViewGetEditor(ULView view);

///
/// Run an editing command.
///
/// @param  editor   The editor.
///
/// @param  command  The command to run.
///
/// @return  Returns whether the command ran (false whenever ulEditorCanExecute() would return
///          false).
///
ULExport bool ulEditorExecute(ULEditor editor, ULEditorCommand command);

///
/// Whether or not a command would do anything right now.
///
/// You can use this to grey out items in a native menu.
///
/// @param  editor   The editor.
///
/// @param  command  The command to check.
///
/// @return  Returns true if ulEditorExecute() would run the command.
///
ULExport bool ulEditorCanExecute(ULEditor editor, ULEditorCommand command);

///
/// Insert text at the caret, replacing the selection (the same as typing it).
///
/// @param  editor  The editor.
///
/// @param  text    The text to insert.
///
/// @return  Returns whether the text was inserted (false when there's no editable text to insert
///          into).
///
ULExport bool ulEditorInsertText(ULEditor editor, ULString text);

///
/// Get the bounds of the caret.
///
/// The rect is in viewport CSS pixels (the same space as ulViewFireMouseEvent()). Multiply by
/// ulViewGetDeviceScale() for device pixels.
///
/// @param  editor  The editor.
///
/// @return  Returns the caret bounds (an empty rect when there's no caret in editable text).
///
/// @note  This updates any pending layout first-- query it when you need it (eg, when opening a
///        menu) rather than every frame.
///
ULExport ULRect ulEditorGetCaretBounds(ULEditor editor);

///
/// A range of a composition's text, in UTF-16 code units (see ulEditorSetComposition()).
///
typedef struct {
  ///
  /// The starting offset of the range, in UTF-16 code units.
  ///
  unsigned int start;

  ///
  /// The ending offset of the range (exclusive), in UTF-16 code units.
  ///
  unsigned int end;
} ULCompositionRange;

///
/// Underline style for part of a composition's text (see ulEditorSetComposition()).
///
/// An input method usually reports one segment per clause, with the clause being converted
/// marked `thick`.
///
/// @note  Zero-initialize this struct. A zero-initialized ULColor is unset.
///
typedef struct {
  ///
  /// The range of composition text this segment styles.
  ///
  ULCompositionRange range;

  ///
  /// The underline color (unset draws the underline in the text's own color).
  ///
  ULColor underline_color;

  ///
  /// A fill drawn behind the text (unset draws no fill).
  ///
  ULColor background_color;

  ///
  /// Whether the underline is drawn thick (the convention for the active clause).
  ///
  bool thick;
} ULCompositionSegment;

///
/// Set or update the in-progress text of an input method's composition.
///
/// The first call starts a composition and replaces the selection. Later calls replace the
/// previous in-progress text.
///
/// @param  editor        The editor.
///
/// @param  text          The full in-progress text. Pass an empty string to remove it (the same
///                       as ulEditorCancelComposition()).
///
/// @param  segments      Underline styles for parts of the text (eg, one per clause). Pass NULL to
///                       underline all of it.
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
ULExport bool ulEditorSetComposition(ULEditor editor, ULString text,
                                     const ULCompositionSegment* segments, size_t num_segments,
                                     ULCompositionRange selection);

///
/// Replace the active composition with the input method's final text.
///
/// With no active composition, this inserts `text` at the caret.
///
/// @param  editor  The editor.
///
/// @param  text    The final text.
///
/// @return  Returns whether the text was inserted (false when there's no editable text).
///
ULExport bool ulEditorCommitComposition(ULEditor editor, ULString text);

///
/// Keep the active composition's in-progress text as final text.
///
/// Call this when the input method closes without committing or cancelling (eg, focus moves away
/// mid-composition).
///
/// @param  editor  The editor.
///
/// @return  Returns whether a composition was finished (false when none was active).
///
ULExport bool ulEditorFinishComposition(ULEditor editor);

///
/// Cancel the active composition and remove its text.
///
/// @param  editor  The editor.
///
/// @return  Returns whether a composition was cancelled (false when none was active).
///
ULExport bool ulEditorCancelComposition(ULEditor editor);

///
/// Whether or not a composition is active.
///
/// @param  editor  The editor.
///
/// @return  Returns true if a composition is active.
///
ULExport bool ulEditorHasComposition(ULEditor editor);

///
/// Get the bounds of the active composition's text, in viewport CSS pixels.
///
/// You can use this as the area a candidate window shouldn't cover.
///
/// @param  editor  The editor.
///
/// @return  Returns the bounds (all-zero when no composition is active).
///
ULExport ULRect ulEditorGetCompositionBounds(ULEditor editor);

///
/// Get the length of the active composition's text, in UTF-16 code units.
///
/// @param  editor  The editor.
///
/// @return  Returns the length (0 when no composition is active).
///
ULExport unsigned int ulEditorGetCompositionCharacterCount(ULEditor editor);

///
/// Get the bounds of one character of the active composition's text, in viewport CSS pixels.
///
/// You can use this to place a candidate window. Anchor it on the composition's first character
/// (index 0) so it doesn't move as the candidates change.
///
/// @param  editor  The editor.
///
/// @param  index   The character's offset within the composition's text, in UTF-16 code units.
///
/// @return  Returns the character's bounds (all-zero when `index` is out of range or no
///          composition is active).
///
ULExport ULRect ulEditorGetCompositionCharacterBounds(ULEditor editor, unsigned int index);

///
/// Get the selection's offsets within the focused element's text.
///
/// The offset functions (this, ulEditorGetTextInRange(), ulEditorReplaceTextInRange(), and
/// ulEditorGetCompositionOffsets()) use UTF-16 code units into the text of the focused text field
/// or editable region. For `<input>` and `<textarea>`, they match the element's `selectionStart`
/// and `selectionEnd`.
///
/// @param  editor  The editor.
///
/// @param  start   Set to the selection's start.
///
/// @param  end     Set to the selection's end (the same as `start` for a caret).
///
/// @return  Returns whether the offsets were set (false when the focused element isn't editable
///          text, or the selection is outside it).
///
ULExport bool ulEditorGetSelectionOffsets(ULEditor editor, unsigned int* start,
                                          unsigned int* end);

///
/// Get part of the focused element's text (see ulEditorGetSelectionOffsets() for the offsets).
///
/// Offsets past the end of the text are clamped to it.
///
/// @param  editor  The editor.
///
/// @param  start   The start offset.
///
/// @param  end     The end offset (exclusive).
///
/// @return  Returns a new string with the text in the range (empty when the focused element isn't
///          editable text). You must call ulDestroyString() when finished.
///
/// @warning  This reads password fields too, so treat the result as sensitive.
///
ULExport ULString ulEditorGetTextInRange(ULEditor editor, unsigned int start, unsigned int end);

///
/// Replace part of the focused element's text (see ulEditorGetSelectionOffsets() for the
/// offsets).
///
/// This edits the same way typing does. The edit is one undo step, and the caret ends up after
/// the new text. You can use this for an input method's replacement ranges (eg, the macOS accent
/// picker replaces the character it targets).
///
/// Offsets past the end of the text are clamped to it.
///
/// @param  editor  The editor.
///
/// @param  start   The start offset.
///
/// @param  end     The end offset (exclusive).
///
/// @param  text    The new text (an empty string deletes the range).
///
/// @return  Returns whether the text was replaced (false when the focused element isn't editable
///          text, or `start` is greater than `end`).
///
/// @note  An active composition is cancelled first, and the offsets apply to the text left after
///        that.
///
ULExport bool ulEditorReplaceTextInRange(ULEditor editor, unsigned int start, unsigned int end,
                                         ULString text);

///
/// Get the active composition's offsets within the focused element's text (see
/// ulEditorGetSelectionOffsets() for the offsets).
///
/// You can use this to report the input method's marked range.
///
/// @param  editor  The editor.
///
/// @param  start   Set to the composition's start.
///
/// @param  end     Set to the composition's end (exclusive).
///
/// @return  Returns whether the offsets were set (false when no composition is active in the
///          focused element).
///
ULExport bool ulEditorGetCompositionOffsets(ULEditor editor, unsigned int* start,
                                            unsigned int* end);

///
/// The kind of element that holds keyboard focus (see ULEditableState).
///
typedef enum {
  kEditableType_None = 0,         ///< No element holds keyboard focus.
  kEditableType_Text,             ///< A single-line text field.
  kEditableType_Password,         ///< A password field.
  kEditableType_TextArea,         ///< A multi-line text area.
  kEditableType_ContentEditable,  ///< An editable (contenteditable) region.
  kEditableType_Other,            ///< A focused element that is not a text-editing target.
} ULEditableType;

///
/// The focused element's inputmode hint (what kind of virtual keyboard the page asks for).
///
typedef enum {
  kInputMode_Unspecified = 0,
  kInputMode_None,
  kInputMode_Text,
  kInputMode_Telephone,
  kInputMode_Url,
  kInputMode_Email,
  kInputMode_Numeric,
  kInputMode_Decimal,
  kInputMode_Search,
} ULInputMode;

///
/// The focused element's enterkeyhint (what label or icon the Enter key should present).
///
typedef enum {
  kEnterKeyHint_Unspecified = 0,
  kEnterKeyHint_Enter,
  kEnterKeyHint_Done,
  kEnterKeyHint_Go,
  kEnterKeyHint_Next,
  kEnterKeyHint_Previous,
  kEnterKeyHint_Search,
  kEnterKeyHint_Send,
} ULEnterKeyHint;

///
/// The conditions that decide whether an input method should be active.
///
/// Reported through the callback you set with ulViewSetChangeEditableStateCallback(). An input
/// method or on-screen keyboard should be active when both `view_has_focus` and `is_mutable` are
/// true.
///
/// @note  This is reported when focus moves, not when the focused element moves or resizes. To
///        follow the caret, use ulEditorGetCaretBounds().
///
typedef struct {
  ///
  /// The kind of element that holds keyboard focus.
  ///
  ULEditableType type;

  ///
  /// Whether or not the focused element accepts typed text (false for a read-only or disabled text
  /// control, and always false when `type` is kEditableType_None or kEditableType_Other).
  ///
  bool is_mutable;

  ///
  /// Whether or not the View itself has input focus (see ulViewFocus() and ulViewUnfocus()).
  ///
  bool view_has_focus;

  ///
  /// The focused element's inputmode hint.
  ///
  ULInputMode input_mode;

  ///
  /// The focused element's enterkeyhint.
  ///
  ULEnterKeyHint enter_key_hint;

  ///
  /// The focused element's bounds in viewport CSS pixels (the same space as
  /// ulViewFireMouseEvent()). All-zero when no element has focus.
  ///
  ULRect bounds;
} ULEditableState;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_EDITOR_H
