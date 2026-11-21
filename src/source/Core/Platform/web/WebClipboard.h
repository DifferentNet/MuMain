#pragma once

// Web build only. The browser's clipboard for the game's text fields. SDL has none on the web, so what
// a field copies (RmlUi's Ctrl+C / Ctrl+X) the page (index.html, window.muClipboard) writes to the
// browser's clipboard too, and a paste comes from the page as typed text.

#include "Core/Platform/web/WebKeyboard.h"

namespace mu::web
{
// The text a game field copied (UTF-8), to the browser's clipboard too. Never a password field's: the
// caller leaves those out, and the page refuses while one has the keys.
void CopyToBrowserClipboard(const char* text);

// Once a frame, with the keys of the field RmlUiRuntime's GetFocusedTextField() finds (None: no field):
// the page takes Ctrl+V and Ctrl+C for a game field only while one has the keys, and never copies out
// of a password field.
void SyncClipboardField(FieldKeys focused);
} // namespace mu::web
