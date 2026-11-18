#include "stdafx.h"
#include "UI/Inventory/ItemGridScroll.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <algorithm>
#include <cmath>

namespace UI::Items::GridScroll
{
namespace
{
constexpr const char* ButtonClass = "item-grid-scroll";
constexpr const char* ButtonSelector = ".item-grid-scroll";
constexpr const char* DirectionAttribute = "data-grid-scroll";
constexpr const char* ViewportId = "grid_viewport";
constexpr const char* CellSelector = ".item-cell";
// Rows of the last page kept in sight on the next, unless the viewport's attribute says otherwise
// (0 for grids stacked a page each, the inventory extension's).
constexpr const char* KeptRowsAttribute = "data-grid-rows-kept";
constexpr int DefaultKeptRows = 1;
// On a button whose way the viewport cannot move any further: the theme dims it (item_grid.rcss).
constexpr const char* AtEndClass = "at-end";

Rml::Element* ScrollButton(Rml::Element* element)
{
    for (; element != nullptr; element = element->GetParentNode())
    {
        if (element->IsClassSet(ButtonClass))
            return element;
    }
    return nullptr;
}

// How far a page moves: the whole rows on view, less the ones kept in sight.
float PageHeight(Rml::Element& viewport)
{
    const float visible = viewport.GetClientHeight();
    Rml::Element* cell = viewport.QuerySelector(CellSelector);
    const float row = cell != nullptr ? cell->GetBox().GetSize(Rml::BoxArea::Margin).y : 0.f;
    if (row <= 0.f)
        return visible;
    const float kept = static_cast<float>(viewport.GetAttribute<int>(KeptRowsAttribute, DefaultKeptRows));
    const float rows = std::max(1.f, std::floor(visible / row) - kept);
    return rows * row;
}

// Marks the paging buttons that cannot move the viewport: up at the first rows, down at the last.
void SyncEnds(Rml::ElementDocument& document)
{
    Rml::Element* viewport = document.GetElementById(ViewportId);
    if (viewport == nullptr)
        return;
    const float top = viewport->GetScrollTop();
    const bool atTop = top <= 0.f;
    // Scroll offsets are whole pixels (Element::SetScrollTop rounds): a pixel short is the end.
    const bool atBottom = top + viewport->GetClientHeight() >= viewport->GetScrollHeight() - 1.f;
    Rml::ElementList buttons;
    document.QuerySelectorAll(buttons, ButtonSelector);
    for (Rml::Element* button : buttons)
    {
        const float direction = button->GetAttribute<float>(DirectionAttribute, 0.f);
        button->SetClass(AtEndClass, direction < 0.f ? atTop : (direction > 0.f && atBottom));
    }
}

class Listener final : public Rml::EventListener
{
public:
    void ProcessEvent(Rml::Event& event) override
    {
        if (event.GetId() != Rml::EventId::Click)
        {
            // The viewport moved (a button, a finger's drag, a window putting it back at the top), the window
            // came up or was laid out at a new size: the buttons follow.
            Rml::Element* target = event.GetTargetElement();
            Rml::ElementDocument* document = target != nullptr ? target->GetOwnerDocument() : nullptr;
            if (document != nullptr && (event.GetId() != Rml::EventId::Scroll || target->GetId() == ViewportId))
                SyncEnds(*document);
            return;
        }
        Rml::Element* button = ScrollButton(event.GetTargetElement());
        Rml::ElementDocument* document = button != nullptr ? button->GetOwnerDocument() : nullptr;
        Rml::Element* viewport = document != nullptr ? document->GetElementById(ViewportId) : nullptr;
        if (viewport == nullptr)
            return;
        const float direction = button->GetAttribute<float>(DirectionAttribute, 0.f);
        viewport->SetScrollTop(viewport->GetScrollTop() + direction * PageHeight(*viewport));
    }
};

Listener& SharedListener()
{
    static Listener listener;
    return listener;
}
} // namespace

void Attach(Rml::ElementDocument* document)
{
    if (document == nullptr || document->QuerySelector(ButtonSelector) == nullptr)
        return;
    document->AddEventListener(Rml::EventId::Click, &SharedListener());
    document->AddEventListener(Rml::EventId::Scroll, &SharedListener());
    document->AddEventListener(Rml::EventId::Show, &SharedListener());
    document->AddEventListener(Rml::EventId::Resize, &SharedListener());
}

void Sync(Rml::ElementDocument* document)
{
    if (document != nullptr)
        SyncEnds(*document);
}
} // namespace UI::Items::GridScroll
