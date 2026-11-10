///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file Dialogs.h
///
/// Native modal message boxes for alerts and confirmations.
///
/// `#include <AppCore/Dialogs.h>`
///
/// This header provides ShowMessageBox() to display modal dialogs from the host OS. The call blocks
/// until the user responds and returns the selected button result.
///
/// You can prompt the user for confirmation before discarding unsaved edits:
///
/// ```
/// bool ConfirmDiscard() {
///   ButtonResult result = ShowMessageBox("Discard changes?",
///                                        "Your unsaved edits will be lost.",
///                                        DialogIcon::Question, ButtonType::YesNo);
///   return result == ButtonResult::Yes;
/// }
/// ```
///
/// ## Main Thread and Linux Fallback
///
/// You must call ShowMessageBox() on the main thread.
///
/// On Linux, the library logs the message and returns the declining button (or ButtonResult::OK
/// when it's the only button) whenever it can't display a dialog:
///
/// - **The call runs on a background thread.**
/// - **The host system doesn't have GTK 3 installed.**
///
/// ## In-Window HTML Dialogs
///
/// To style dialogs with HTML and CSS instead of native OS chrome, create a floating panel through
/// Foreground. Setting FocusPolicy::Grab on the panel ensures it takes keyboard focus whenever it's
/// shown.
///
/// @see  ShowMessageBox(), Foreground, FocusPolicy
///

#pragma once

#include "Defines.h"
#include <Ultralight/String.h>

namespace ultralight {

///
/// Enum representing the icon to be displayed in a dialog.
///
enum class DialogIcon {
  Info,     ///< Information icon
  Warning,  ///< Warning icon
  Error,    ///< Error icon
  Question  ///< Question icon
};

///
/// Enum representing the button types in a dialog.
///
enum class ButtonType {
  OK,        ///< Single "OK" button
  OKCancel,  ///< "OK" and "Cancel" buttons
  YesNo      ///< "Yes" and "No" buttons
};

///
/// Enum representing the result of a dialog button press.
///
enum class ButtonResult {
  OK,      ///< "OK" button was pressed
  Cancel,  ///< "Cancel" button was pressed
  Yes,     ///< "Yes" button was pressed
  No       ///< "No" button was pressed
};

///
/// Show a modal message box with the specified title, message, icon, and buttons.
///
/// @param  title    The title of the message box.
///
/// @param  message  The message to display in the message box.
///
/// @param  icon     The icon to display in the message box (default: DialogIcon::Info).
///
/// @param  buttons  The button type to display in the message box (default: ButtonType::OK).
///
/// @return  Returns the result of the button press.
///
AExport ButtonResult ShowMessageBox(const String& title, const String& message,
                                    DialogIcon icon = DialogIcon::Info,
                                    ButtonType buttons = ButtonType::OK);

} // namespace ultralight