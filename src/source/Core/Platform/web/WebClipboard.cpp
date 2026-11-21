// Web build only: see WebClipboard.h.

#include "Core/Platform/web/WebClipboard.h"

#include <emscripten.h>

namespace
{
// The field the page was last told of (SyncClipboardField); -1 before the first frame.
int g_field = -1;

EM_JS(void, MuWeb_CopyToClipboard, (const char* text), {
    if (window.muClipboard)
        window.muClipboard.copy(UTF8ToString(text));
});

// field: a FieldKeys.
EM_JS(void, MuWeb_SetClipboardField, (int field), {
    if (window.muClipboard)
        window.muClipboard.field = field;
});
} // namespace

void mu::web::CopyToBrowserClipboard(const char* text)
{
    if (text == nullptr || text[0] == '\0')
        return;

    MuWeb_CopyToClipboard(text);
}

void mu::web::SyncClipboardField(FieldKeys focused)
{
    const int field = static_cast<int>(focused);
    if (field == g_field)
        return;

    g_field = field;
    MuWeb_SetClipboardField(field);
}
