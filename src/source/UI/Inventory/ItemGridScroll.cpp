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

class Listener final : public Rml::EventListener
{
public:
    void ProcessEvent(Rml::Event& event) override
    {
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
}
} // namespace UI::Items::GridScroll
