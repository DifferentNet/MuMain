#include "stdafx.h"
#include "App/Platform/AttentionSignal.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include "Core/Platform/GameWindow.h"

#include <SDL3/SDL_video.h>

extern bool g_bHasInputFocus;
extern bool g_bWindowMinimized;
#endif

namespace
{
#ifdef __EMSCRIPTEN__
// The page decides by itself whether it is in front: a hidden tab does not always lose the focus first. The title
// mark goes on once (a second request while it is there changes nothing) and comes off on the next focus.
EM_JS(void, MuWeb_RequestAttention, (), {
    if (document.hasFocus() && !document.hidden)
        return;
    if (Module["muAttentionTitle"] !== undefined)
        return;
    var mark = "(!) ";
    Module["muAttentionTitle"] = document.title;
    document.title = mark + document.title;
    var restore = function() {
        if (document.hidden)
            return;
        window.removeEventListener("focus", restore);
        document.removeEventListener("visibilitychange", restore);
        if (Module["muAttentionTitle"] === undefined)
            return;
        if (document.title.indexOf(mark) === 0)
            document.title = Module["muAttentionTitle"];
        Module["muAttentionTitle"] = undefined;
    };
    window.addEventListener("focus", restore);
    document.addEventListener("visibilitychange", restore);
});
#endif
} // namespace

void App::Platform::Attention::Request()
{
#ifdef __EMSCRIPTEN__
    MuWeb_RequestAttention();
#else
    if (g_bHasInputFocus && !g_bWindowMinimized)
        return;
    SDL_Window* window = Core::Platform::GameWindow();
    if (window == nullptr)
        return;
    SDL_FlashWindow(window, SDL_FLASH_UNTIL_FOCUSED);
#endif
}
