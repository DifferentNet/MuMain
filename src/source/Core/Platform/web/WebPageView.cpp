// Web build only: see WebPageView.h.

#include "Core/Platform/web/WebPageView.h"

#include <emscripten.h>

namespace
{
EM_JS(int, MuWeb_TakeViewRequest, (), {
    const view = window.muView;
    if (!view || typeof view.request != "number")
        return -1;
    const request = view.request;
    view.request = null;
    return request;
});

EM_JS(void, MuWeb_ShowView, (int choice, int phoneView), {
    if (window.muView && window.muView.show)
        window.muView.show(choice, !!phoneView);
});

EM_JS(void, MuWeb_ShowInWorld, (int inWorld), {
    if (window.muInWorld)
        window.muInWorld(!!inWorld);
});
} // namespace

int mu::web::TakePageViewRequest()
{
    return MuWeb_TakeViewRequest();
}

void mu::web::ShowPageView(int choice, bool phoneView)
{
    MuWeb_ShowView(choice, phoneView ? 1 : 0);
}

void mu::web::ShowInWorld(bool inWorld)
{
    static bool shown = false;
    if (inWorld == shown)
        return;
    shown = inWorld;
    MuWeb_ShowInWorld(inWorld ? 1 : 0);
}
