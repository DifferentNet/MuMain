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

EM_JS(void, MuWeb_ShowInWorld, (int inWorld, const char* title), {
    if (window.muInWorld)
        window.muInWorld(!!inWorld, UTF8ToString(title));
});

EM_JS(void, MuWeb_ShowGameEnded, (), {
    if (window.muGameEnded)
        window.muGameEnded();
});

EM_JS(int, MuWeb_TakeShotRequest, (), {
    if (!window.muShotRequest)
        return 0;
    window.muShotRequest = false;
    return 1;
});

EM_JS(void, MuWeb_ShowShotFailed, (), {
    if (window.muShotFailed)
        window.muShotFailed();
});

EM_JS(void, MuWeb_OfferShot, (const char* path), {
    const file = UTF8ToString(path);
    try {
        const bytes = FS.readFile(file);
        if (window.muShotReady)
            window.muShotReady(bytes);
    } catch (e) {
        console.warn("Screenshot not read:", e);
    }
    try {
        FS.unlink(file);
    } catch (e) {}
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

void mu::web::ShowInWorld(bool inWorld, const char* title)
{
    MuWeb_ShowInWorld(inWorld ? 1 : 0, title);
}

void mu::web::ShowGameEnded()
{
    MuWeb_ShowGameEnded();
}

bool mu::web::TakePageShotRequest()
{
    return MuWeb_TakeShotRequest() != 0;
}

void mu::web::OfferShot(const char* path)
{
    MuWeb_OfferShot(path);
}

void mu::web::ShowShotFailed()
{
    MuWeb_ShowShotFailed();
}
