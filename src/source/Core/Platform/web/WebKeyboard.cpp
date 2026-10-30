// Web build only: see WebKeyboard.h.

#include "Core/Platform/web/WebKeyboard.h"

#include <emscripten.h>

namespace
{
using mu::web::FieldKeys;

// Whether the page was asked to show the keyboard and not to hide it since, and for which field.
bool g_keyboardShown = false;
FieldKeys g_keys = FieldKeys::Text;
float g_lineTop = 0.0f;
float g_lineBottom = 0.0f;
// A gamepad is in use (HoldBackKeyboard), and whether a field wants the keyboard meanwhile.
bool g_heldBack = false;
bool g_wanted = false;

// What the page was last told about the field under a tapping finger (SyncTapOnTextField).
FieldKeys g_tapKeys = FieldKeys::None;

// keys: a FieldKeys. "fresh": a field that just took the keys, so the page starts its own input over.
EM_JS(void, MuWeb_ShowKeyboard, (int keys, float lineTop, float lineBottom, int fresh), {
    if (Module.muKeyboard)
        Module.muKeyboard.show(keys, lineTop, lineBottom, fresh != 0);
});

EM_JS(void, MuWeb_HideKeyboard, (), {
    if (Module.muKeyboard)
        Module.muKeyboard.hide();
});

// keys: a FieldKeys.
EM_JS(void, MuWeb_TapOnTextField, (int keys), {
    if (Module.muKeyboard)
        Module.muKeyboard.tapOnField(keys);
});

void ShowFieldKeyboard(FieldKeys keys, float lineTop, float lineBottom)
{
    g_keys = keys;
    g_lineTop = lineTop;
    g_lineBottom = lineBottom;
    if (g_heldBack)
    {
        g_wanted = true;
        return;
    }
    MuWeb_ShowKeyboard(static_cast<int>(keys), lineTop, lineBottom, g_keyboardShown ? 0 : 1);
    g_keyboardShown = true;
}
} // namespace

void mu::web::ShowKeyboard(bool password, float lineTop, float lineBottom)
{
    // Not text keys for a number field as its caret moves, to be put right at the end of the frame.
    FieldKeys keys = g_keys;
    if (password)
        keys = FieldKeys::Password;
    else if (keys == FieldKeys::Password)
        keys = FieldKeys::Text;
    ShowFieldKeyboard(keys, lineTop, lineBottom);
}

void mu::web::HideKeyboard()
{
    g_wanted = false;
    if (!g_keyboardShown)
        return;

    g_keyboardShown = false;
    MuWeb_HideKeyboard();
}

void mu::web::SyncKeyboard(FieldKeys focused)
{
    if (focused == FieldKeys::None)
        HideKeyboard();
    else if (!g_heldBack && g_wanted && !g_keyboardShown)
        ShowFieldKeyboard(focused, g_lineTop, g_lineBottom);
    else if (g_keyboardShown && focused != g_keys)
        ShowFieldKeyboard(focused, g_lineTop, g_lineBottom);
}

void mu::web::SyncTapOnTextField(FieldKeys tapped)
{
    const FieldKeys keys = g_heldBack ? FieldKeys::None : tapped;
    if (keys == g_tapKeys)
        return;
    g_tapKeys = keys;
    MuWeb_TapOnTextField(static_cast<int>(keys));
}

void mu::web::HoldBackKeyboard(bool heldBack)
{
    if (heldBack == g_heldBack)
        return;
    g_heldBack = heldBack;
    if (!heldBack)
        return;
    const bool shown = g_keyboardShown;
    HideKeyboard();
    g_wanted = shown;
}
