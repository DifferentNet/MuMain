#include "stdafx.h"
#include "UiInputRouter.h"

namespace Core::Input
{
    namespace
    {
        IUiInputConsumer* g_pUiInputConsumer = nullptr;
    }

    void SetUiInputConsumer(IUiInputConsumer* consumer)
    {
        g_pUiInputConsumer = consumer;
    }

    bool RouteToUi(SDL_Event& event, SDL_Window* window)
    {
        return g_pUiInputConsumer ? g_pUiInputConsumer->ProcessSdlEvent(event, window) : true;
    }

    IUiInputConsumer* ActiveUiInputConsumer()
    {
        return g_pUiInputConsumer;
    }

    void CancelSyntheticMousePress(IUiInputConsumer* owner, unsigned char button, SDL_Window* window)
    {
        if (owner && owner == g_pUiInputConsumer)
            owner->CancelSyntheticMousePress(button, window);
    }

    bool IsMouseOverUI()
    {
        return g_pUiInputConsumer && g_pUiInputConsumer->IsMouseOverUI();
    }

    bool BeginUiTouchScroll(float x, float y)
    {
        return g_pUiInputConsumer && g_pUiInputConsumer->BeginTouchScroll(x, y);
    }

    void MoveUiTouchScroll(float fromX, float fromY, float toX, float toY)
    {
        if (g_pUiInputConsumer)
            g_pUiInputConsumer->MoveTouchScroll(fromX, fromY, toX, toY);
    }

    void EndUiTouchScroll(float x, float y, float velocityX, float velocityY)
    {
        if (g_pUiInputConsumer)
            g_pUiInputConsumer->EndTouchScroll(x, y, velocityX, velocityY);
    }

    bool StopUiTouchScrollGlide()
    {
        return g_pUiInputConsumer && g_pUiInputConsumer->StopTouchScrollGlide();
    }
}
