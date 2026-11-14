#include "stdafx.h"
#include "UI/RmlBridge/RmlKeptElement.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

Rml::Element* UI::RmlBridge::KeptElement::Find(Rml::ElementDocument* document, const char* id)
{
    if (document == nullptr)
        return nullptr;
    if (Rml::Element* element = m_element.get();
        element != nullptr && element->GetOwnerDocument() == document && element->GetId() == id)
        return element;
    Rml::Element* element = document->GetElementById(id);
    m_element = element != nullptr ? element->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>{};
    return element;
}
