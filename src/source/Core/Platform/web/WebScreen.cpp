// Web build only: see WebScreen.h.

#include "Core/Platform/web/WebScreen.h"

#include <emscripten.h>

namespace
{
// which: 0/1 the canvas's width/height on the page in CSS pixels, 2 its height in its own pixels.
EM_JS(double, MuWeb_CanvasSize, (int which), {
    const canvas = Module.canvas || document.getElementById("canvas");
    if (!canvas)
        return 0;
    const box = canvas.getBoundingClientRect();
    const sizes = [ box.width, box.height, canvas.height ];
    return sizes[which];
});

// Module.muWake: whether the game wants the screen on, and the lock while held. The browser lets the
// lock go when the page is hidden; it is asked for again when the page shows.
EM_JS(void, MuWeb_KeepScreenOn, (int on), {
    if (!navigator.wakeLock)
        return;
    let wake = Module.muWake;
    if (!wake)
    {
        wake = Module.muWake = {want : false, lock : null, asking : false};
        wake.take = function()
        {
            if (!wake.want || wake.lock || wake.asking || document.visibilityState != "visible")
                return;
            wake.asking = true;
            navigator.wakeLock.request("screen")
                .then(function(lock) {
                    wake.asking = false;
                    wake.lock = lock;
                    lock.addEventListener(
                        "release", function() {
                            if (wake.lock == lock)
                                wake.lock = null;
                        });
                    if (!wake.want)
                        wake.drop();
                })
                .catch(function() { wake.asking = false; });
        };
        wake.drop = function()
        {
            const lock = wake.lock;
            wake.lock = null;
            if (lock)
                lock.release().catch(function(){});
        };
        document.addEventListener("visibilitychange", wake.take);
    }
    wake.want = !!on;
    if (wake.want)
        wake.take();
    else
        wake.drop();
});

// Module.muBack: the entry the page keeps on top of its history while armed, and the presses counted.
// Stepping back from it fires popstate on the same page: a press, and the entry goes on top again.
EM_JS(void, MuWeb_ArmBack, (int armed), {
    let back = Module.muBack;
    if (!back)
    {
        back = Module.muBack = {armed : false, pushed : false, presses : 0};
        back.push = function()
        {
            if (back.armed && !back.pushed)
            {
                history.pushState({muBack : true}, "");
                back.pushed = true;
            }
        };
        window.addEventListener(
            "popstate", function() {
                if (!back.pushed)
                    return;
                back.pushed = false;
                if (!back.armed)
                    return;
                back.presses++;
                back.push();
            });
    }
    back.armed = !!armed;
    back.push();
});

EM_JS(int, MuWeb_TakeBackPresses, (), {
    const back = Module.muBack;
    if (!back)
        return 0;
    const presses = back.presses;
    back.presses = 0;
    return presses;
});
} // namespace

bool mu::web::MeasureCanvas(CanvasOnPage& out)
{
    const double width = MuWeb_CanvasSize(0);
    const double height = MuWeb_CanvasSize(1);
    const double pixels = MuWeb_CanvasSize(2);
    if (width <= 0.0 || height <= 0.0 || pixels <= 0.0)
        return false;
    out.cssWidth = static_cast<float>(width);
    out.cssHeight = static_cast<float>(height);
    out.pixelsPerCss = static_cast<float>(pixels / height);
    return true;
}

void mu::web::KeepScreenOn(bool on)
{
    MuWeb_KeepScreenOn(on ? 1 : 0);
}

void mu::web::ArmBackGesture(bool armed)
{
    MuWeb_ArmBack(armed ? 1 : 0);
}

int mu::web::TakeBackPresses()
{
    return MuWeb_TakeBackPresses();
}
