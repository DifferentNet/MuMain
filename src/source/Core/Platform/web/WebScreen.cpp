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

// Module.muInput: the page's input events counted, seen first by the window (capture) and never held
// up (passive), so the game's own handlers get them as before. `down`: the contacts down now (a finger,
// a pen, a mouse button) by pointer id, each with the time of its last event. A lift, a cancel or a
// lost capture takes one off; leaving the page (blur, hidden) takes them all, and a mouse or pen
// moving with no button down its own.
EM_JS(int, MuWeb_TakePageInputs, (), {
    let input = Module.muInput;
    if (!input)
    {
        input = Module.muInput = {count : 0, down : new Map()};
        const listen = {capture : true, passive : true};
        const count = function() { input.count++; };
        for (const type of ["pointerdown", "pointermove", "touchstart", "keydown", "wheel"])
            window.addEventListener(type, count, listen);
        window.addEventListener(
            "pointerdown", function(event) { input.down.set(event.pointerId, performance.now()); }, listen);
        window.addEventListener(
            "pointermove", function(event) {
                if (!input.down.has(event.pointerId))
                    return;
                if (event.buttons)
                    input.down.set(event.pointerId, performance.now());
                else
                    input.down.delete(event.pointerId);
            }, listen);
        const lift = function(event) { input.down.delete(event.pointerId); };
        for (const type of ["pointerup", "pointercancel", "lostpointercapture"])
            window.addEventListener(type, lift, listen);
        window.addEventListener("blur", function() { input.down.clear(); });
        document.addEventListener(
            "visibilitychange", function() {
                if (document.hidden)
                    input.down.clear();
            });
    }
    const count = input.count;
    input.count = 0;
    return count;
});

// Whether a contact is down on the page (Module.muInput.down); one with no event for 10 minutes counts
// as lifted, so an end the browser never sent cannot keep the game awake for good.
EM_JS(int, MuWeb_PageContactDown, (), {
    const input = Module.muInput;
    if (!input || !input.down.size)
        return 0;
    const LONGEST_STILL_MS = 10 * 60 * 1000;
    const now = performance.now();
    for (const [id, at] of input.down)
    {
        if (now - at > LONGEST_STILL_MS)
            input.down.delete(id);
    }
    return input.down.size > 0 ? 1 : 0;
});

// Module.muBack: the entry the page keeps on top of its history while armed (marked with this page's own
// token), and the presses counted. Stepping back from it fires popstate on the same page: a press, and the
// page steps forward onto its entry again. It added a new entry after each press before: Chrome's Back skips
// the entries a page adds without a tap or a key, and after a step back it counts none until the next one
// (its history manipulation intervention), so a second Back with no tap between left the game, and closed
// the installed app. For the same reason the entry is added in a tap's or a key's event (a user activation),
// or at once where there was one since the page loaded and it has not stepped through its history yet (the
// tap on Play): Chrome counts a tap or a key since the last step. A step forward that doesn't land (no entry
// ahead) adds a new entry at once, as before.
EM_JS(void, MuWeb_ArmBack, (int armed), {
    let back = Module.muBack;
    if (!back)
    {
        // How long a step forward may take before a new entry is added instead.
        const FORWARD_WAIT_MS = 500;
        back = Module.muBack = {
            armed : false,
            onTop : false,
            stepped : false,
            forwardTimer : 0,
            presses : 0,
            token : Math.random()
        };
        back.push = function()
        {
            history.pushState({muBack : back.token}, "");
            back.onTop = true;
        };
        // Where the browser does not say whether there was a user activation, the entry is added at once.
        back.pushOnActivation = function()
        {
            const activation = navigator.userActivation;
            const active = !activation || activation.isActive || (activation.hasBeenActive && !back.stepped);
            if (back.armed && !back.onTop && !back.forwardTimer && active)
                back.push();
        };
        const listen = {capture : true, passive : true};
        for (const type of ["pointerup", "touchend", "mousedown", "keydown"])
            window.addEventListener(type, back.pushOnActivation, listen);
        window.addEventListener(
            "popstate", function(event) {
                back.stepped = true;
                if (event.state && event.state.muBack === back.token)
                {
                    // On the entry again: the step forward landed, or the browser stepped forward itself.
                    back.onTop = true;
                    clearTimeout(back.forwardTimer);
                    back.forwardTimer = 0;
                    return;
                }
                if (!back.onTop)
                    return;
                back.onTop = false;
                if (!back.armed)
                    return;
                back.presses++;
                if (window.navigation && !navigation.canGoForward)
                {
                    back.push();
                    return;
                }
                history.forward();
                back.forwardTimer = setTimeout(function() {
                    back.forwardTimer = 0;
                    if (back.armed && !back.onTop)
                        back.push();
                }, FORWARD_WAIT_MS);
            });
    }
    back.armed = !!armed;
    back.pushOnActivation();
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

int mu::web::TakePageInputs()
{
    return MuWeb_TakePageInputs();
}

bool mu::web::IsPageContactDown()
{
    return MuWeb_PageContactDown() != 0;
}

void mu::web::ArmBackGesture(bool armed)
{
    MuWeb_ArmBack(armed ? 1 : 0);
}

int mu::web::TakeBackPresses()
{
    return MuWeb_TakeBackPresses();
}
