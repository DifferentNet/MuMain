#include "stdafx.h"
#include "RmlUiSystemInterface.h"

#include "Core/Utilities/Log/MuLogger.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <SDL3/SDL.h>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebClipboard.h"
#include "Core/Platform/web/WebKeyboard.h"
#endif

namespace
{
    // RmlUi animation and double-click timing must advance independently of game pause/ticks.
    const std::chrono::steady_clock::time_point g_StartTime = std::chrono::steady_clock::now();

    bool IsPasswordField(const Rml::Element* field)
    {
        return field && field->GetAttribute<Rml::String>("type", "") == "password";
    }

    // RmlUi copies a password field's real text on Ctrl+C / Ctrl+X, though the field shows only dots.
    bool CopyingFromPasswordField()
    {
        Rml::Context* context = Rml::GetContext("main");
        return context && IsPasswordField(context->GetFocusElement());
    }

#ifdef __EMSCRIPTEN__
    // A phone has no keys until its own keyboard comes up (Core/Platform/web/WebKeyboard.h). RmlUi
    // focuses the field before it activates the keyboard, and gives its line in the context's pixels.
    void ShowPhoneKeyboard(float lineTop, float lineHeight)
    {
        Rml::Context* context = Rml::GetContext("main");
        if (!context)
            return;

        const bool password = IsPasswordField(context->GetFocusElement());
        const int height = context->GetDimensions().y;
        const float scale = height > 0 ? 1.0f / static_cast<float>(height) : 0.0f;
        mu::web::ShowKeyboard(password, lineTop * scale, (lineTop + lineHeight) * scale);
    }
#endif
}

RmlUiSystemInterface::RmlUiSystemInterface(SDL_Window* window) : m_Window(window)
{
}

double RmlUiSystemInterface::GetElapsedTime()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_StartTime).count();
}

bool RmlUiSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
    const auto logger = mu::log::Get("ui");
    switch (type)
    {
        case Rml::Log::LT_ERROR:
        case Rml::Log::LT_ASSERT:  MU_LOG_ERROR(logger, "[RmlUi] {}", message); break;
        case Rml::Log::LT_WARNING: MU_LOG_WARN(logger, "[RmlUi] {}", message);  break;
        case Rml::Log::LT_DEBUG:   MU_LOG_DEBUG(logger, "[RmlUi] {}", message); break;
        default:                   MU_LOG_INFO(logger, "[RmlUi] {}", message);  break;
    }
    return true; // continue execution -- never break into the debugger from here
}

void RmlUiSystemInterface::SetClipboardText(const Rml::String& text)
{
    if (CopyingFromPasswordField())
        return;
    // SDL3's clipboard API (UTF-8, cross-platform) rather than raw Win32 OpenClipboard/
    // SetClipboardData -- consistent with this engine's SDL-based platform abstraction and its
    // existing portability discipline (GdiText.cpp etc.).
    SDL_SetClipboardText(text.c_str());
#ifdef __EMSCRIPTEN__
    // SDL has no clipboard of its own on the web: the page writes it to the browser's too.
    mu::web::CopyToBrowserClipboard(text.c_str());
#endif
}

void RmlUiSystemInterface::GetClipboardText(Rml::String& text)
{
    char* clipboard = SDL_GetClipboardText();
    text = clipboard ? clipboard : "";
    if (clipboard) SDL_free(clipboard);
}

void RmlUiSystemInterface::ActivateKeyboard(Rml::Vector2f caret_position, float line_height)
{
    // RmlUi supplies the caret rectangle in window pixels, as SDL_SetTextInputArea requires.
    m_TextInputActive = true;
    if (!m_Window) return;
    const SDL_Rect rect = {static_cast<int>(caret_position.x), static_cast<int>(caret_position.y), 1, static_cast<int>(line_height)};
    SDL_SetTextInputArea(m_Window, &rect, 0);
    SDL_StartTextInput(m_Window);
#ifdef __EMSCRIPTEN__
    ShowPhoneKeyboard(caret_position.y, line_height);
#endif
}

void RmlUiSystemInterface::DeactivateKeyboard()
{
    m_TextInputActive = false;
#ifdef __EMSCRIPTEN__
    mu::web::HideKeyboard();
#endif
    if (!m_Window) return;
    SDL_StopTextInput(m_Window);
}
