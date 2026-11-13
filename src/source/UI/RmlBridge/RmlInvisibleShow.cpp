#include "stdafx.h"
#include "UI/RmlBridge/RmlInvisibleShow.h"

#ifdef __EMSCRIPTEN__
#include <RmlUi/Core/StyleTypes.h>

namespace UI::RmlBridge
{
bool InvisibleShow::Begin(Rml::ElementDocument* document)
{
    End();
    if (document == nullptr || document->IsVisible())
        return false;

    m_saved = {SavedProperty{Rml::PropertyId::Visibility}, SavedProperty{Rml::PropertyId::Opacity},
               SavedProperty{Rml::PropertyId::PointerEvents}};
    for (SavedProperty& saved : m_saved)
    {
        if (const Rml::Property* local = document->GetLocalProperty(saved.id))
            saved.value = *local;
    }

    // Opacity 0 still lays out, loads and renders everything (RmlUi skips no element for it); with
    // pointer-events none no pointer or finger hits the document.
    document->SetProperty(Rml::PropertyId::Opacity, Rml::Property(0.f, Rml::Unit::NUMBER));
    document->SetProperty(Rml::PropertyId::PointerEvents, Rml::Property(Rml::Style::PointerEvents::None));
    document->SetProperty(Rml::PropertyId::Visibility, Rml::Property(Rml::Style::Visibility::Visible));
    m_document = document->GetObserverPtr();
    m_active = true;
    return true;
}

void InvisibleShow::End()
{
    if (!m_active)
        return;
    m_active = false;
    Rml::Element* element = m_document.get();
    m_document.reset();
    auto* document = element != nullptr ? element->GetOwnerDocument() : nullptr;
    if (document == nullptr)
        return;

    for (const SavedProperty& saved : m_saved)
    {
        if (saved.value)
            document->SetProperty(saved.id, *saved.value);
        else
            document->RemoveProperty(saved.id);
    }
    // The windows ask IsVisible() before the next Context::Update(): it answers from the computed style.
    document->UpdateDocument();
}
} // namespace UI::RmlBridge
#endif
