#pragma once

// The browser client's window warm-up (UI/Core/WindowWarmUp.h): a hidden document shown for one RmlUi frame
// without being seen or touched, so that its first layout, images and geometry are done before the player
// opens it. Begin() before Context::Update(), End() after Context::Render(), in the same frame: the game's
// own logic never sees the document visible.
//
// Only the document's visibility, opacity and pointer-events change, as inline properties, and End() puts
// back what was there. Unlike ElementDocument::Show()/Hide(), nothing focuses, no show/hide event goes out,
// and no other document gets the focus back (Context::UnfocusDocument()).
#ifdef __EMSCRIPTEN__
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ObserverPtr.h>
#include <RmlUi/Core/Property.h>

#include <array>
#include <optional>

namespace UI::RmlBridge
{
class InvisibleShow
{
public:
    // False when the document is missing or already visible; nothing changes then.
    bool Begin(Rml::ElementDocument* document);
    // Puts the document back as Begin() found it; nothing when it is gone (a theme switch).
    void End();
    bool IsActive() const { return m_active; }

private:
    struct SavedProperty
    {
        Rml::PropertyId id;
        std::optional<Rml::Property> value;
    };

    Rml::ObserverPtr<Rml::Element> m_document;
    std::array<SavedProperty, 3> m_saved{};
    bool m_active = false;
};
} // namespace UI::RmlBridge
#endif
