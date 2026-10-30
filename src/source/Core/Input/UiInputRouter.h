#pragma once

union SDL_Event;
struct SDL_Window;

namespace Core::Input
{
    // Abstract seam between the platform's raw SDL input and whatever UI framework claims it
    // first. Exists so the call sites that decide "does this event/mouse-position belong to the
    // UI right now" (Winmain.cpp's event pump, gameplay's mouse-gating checks in Selection.cpp /
    // ZzzInterface.cpp) depend on this interface, not on a concrete UI framework type. RmlUi
    // (RmlUiRuntime) is the only implementation today, but nothing outside RmlUiRuntime itself
    // needs to know that -- SDL stays independent of RmlUi, and the UI framework could
    // theoretically be replaced by swapping the registered consumer alone.
    class IUiInputConsumer
    {
    public:
        virtual ~IUiInputConsumer() = default;

        // @return false if the UI claimed this event (an element under the cursor / holding
        // focus consumed it) -- the caller should skip its own legacy/gameplay handling for this
        // event. true means the UI did not consume it and normal handling should proceed. Matches
        // Rml::Context::Process*'s own "still propagating" convention so implementations can
        // forward it directly.
        virtual bool ProcessSdlEvent(SDL_Event& event, SDL_Window* window) = 0;

        // Retract this consumer's synthetic press without dispatching a click.
        virtual void CancelSyntheticMousePress(unsigned char button, SDL_Window* window) = 0;

        // True while the mouse is hovering/pressed over any element the registered consumer owns.
        virtual bool IsMouseOverUI() const = 0;

        // Touch scrolling, as on a phone: a finger dragged over a scrollable list scrolls it instead
        // of dragging the mouse (Touch::Pointer). Points are window pixels. A UI that keeps these
        // defaults scrolls nothing by touch: the finger drags the mouse there too.
        // @return true if a scrollable box lies under the point where the finger landed; that box
        // then follows MoveTouchScroll() until EndTouchScroll().
        virtual bool BeginTouchScroll(float /*x*/, float /*y*/)
        {
            return false;
        }
        // The finger moved from one point to another.
        virtual void MoveTouchScroll(float /*fromX*/, float /*fromY*/, float /*toX*/, float /*toY*/) {}
        // The finger lifted at (x, y), moving at (velocityX, velocityY) pixels a second: a flick
        // glides on.
        virtual void EndTouchScroll(float /*x*/, float /*y*/, float /*velocityX*/, float /*velocityY*/) {}
        // A finger landed: a box still gliding stops where it is. @return true if one was.
        virtual bool StopTouchScrollGlide()
        {
            return false;
        }
        // Whether a list or a box under the window point scrolls up and down: the mouse wheel there
        // moves it, so the pad's d-pad turns the wheel instead of pressing its controls.
        virtual bool ScrollsAt(float /*x*/, float /*y*/) const
        {
            return false;
        }
    };

    // Registers the single active UI input consumer. Set once at startup (RmlUiRuntime::Create())
    // and cleared at shutdown (RmlUiRuntime::Destroy()); nullptr is a valid "no UI yet" state.
    void SetUiInputConsumer(IUiInputConsumer* consumer);

    // Routes one SDL event to the registered UI consumer, if any. Returns true (event still
    // propagating) when no consumer is registered, matching IUiInputConsumer::ProcessSdlEvent's
    // own "not consumed" convention -- callers don't need to null-check.
    bool RouteToUi(SDL_Event& event, SDL_Window* window);
    IUiInputConsumer* ActiveUiInputConsumer();
    void CancelSyntheticMousePress(IUiInputConsumer* owner, unsigned char button, SDL_Window* window);

    // True if the registered UI consumer reports the mouse is over UI. False if none registered.
    bool IsMouseOverUI();

    // The registered consumer's touch scrolling (IUiInputConsumer::BeginTouchScroll() and the
    // rest); with none registered nothing scrolls.
    bool BeginUiTouchScroll(float x, float y);
    void MoveUiTouchScroll(float fromX, float fromY, float toX, float toY);
    void EndUiTouchScroll(float x, float y, float velocityX, float velocityY);
    bool StopUiTouchScrollGlide();
    // The registered consumer's IUiInputConsumer::ScrollsAt(); with none registered nothing scrolls.
    bool UiScrollsAt(float x, float y);
}
