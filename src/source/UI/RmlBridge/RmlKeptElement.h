#pragma once

#include <RmlUi/Core/ObserverPtr.h>

namespace Rml
{
    class Element;
    class ElementDocument;
}

namespace UI::RmlBridge
{
    // An element found by its id once and kept, so a lookup made every frame skips the search through the
    // document. The kept element is used while it lives in that document under that id; one destroyed (a
    // document built anew), moved or given another id is looked for again. An id the document lacks is not
    // remembered: it is searched for on every call, as without the cache.
    class KeptElement
    {
    public:
        Rml::Element* Find(Rml::ElementDocument* document, const char* id);

    private:
        Rml::ObserverPtr<Rml::Element> m_element;
    };
}
