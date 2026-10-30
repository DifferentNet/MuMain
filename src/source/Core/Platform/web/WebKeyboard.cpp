// Web build only: see WebKeyboard.h.

#include "Core/Platform/web/WebKeyboard.h"

#include <emscripten.h>

namespace
{
// Whether the page was asked to show the keyboard and not to hide it since, and for which field.
bool g_keyboardShown = false;
bool g_password = false;
float g_lineTop = 0.0f;
float g_lineBottom = 0.0f;
// A gamepad is in use (HoldBackKeyboard), and whether a field wants the keyboard meanwhile.
bool g_heldBack = false;
bool g_wanted = false;

// "fresh": a field that just took the keys, so the page starts its own input over.
EM_JS(void, MuWeb_ShowKeyboard, (int password, float lineTop, float lineBottom, int fresh), {
    if (Module.muKeyboard)
        Module.muKeyboard.show(password != 0, lineTop, lineBottom, fresh != 0);
});

EM_JS(void, MuWeb_HideKeyboard, (), {
    if (Module.muKeyboard)
        Module.muKeyboard.hide();
});
} // namespace

void mu::web::ShowKeyboard(bool password, float lineTop, float lineBottom)
{
    g_password = password;
    g_lineTop = lineTop;
    g_lineBottom = lineBottom;
    if (g_heldBack)
    {
        g_wanted = true;
        return;
    }
    MuWeb_ShowKeyboard(password ? 1 : 0, lineTop, lineBottom, g_keyboardShown ? 0 : 1);
    g_keyboardShown = true;
}

void mu::web::HideKeyboard()
{
    g_wanted = false;
    if (!g_keyboardShown)
        return;

    g_keyboardShown = false;
    MuWeb_HideKeyboard();
}

void mu::web::SyncKeyboard(bool textFieldFocused, bool password)
{
    if (!textFieldFocused)
        HideKeyboard();
    else if (!g_heldBack && g_wanted && !g_keyboardShown)
        ShowKeyboard(password, g_lineTop, g_lineBottom);
    else if (g_keyboardShown && password != g_password)
        ShowKeyboard(password, g_lineTop, g_lineBottom);
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
