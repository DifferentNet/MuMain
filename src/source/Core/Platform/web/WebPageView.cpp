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
} // namespace

int mu::web::TakePageViewRequest()
{
    return MuWeb_TakeViewRequest();
}

void mu::web::ShowPageView(int choice, bool phoneView)
{
    MuWeb_ShowView(choice, phoneView ? 1 : 0);
}
