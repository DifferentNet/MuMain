#include "stdafx.h"
#include "Render/RmlUi/RmlTouchScroll.h"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ScrollTypes.h>

#include <algorithm>

namespace Render::RmlUi
{
namespace
{
// A flick glides this long at the speed the finger lifted with: that far, slowing as it goes.
constexpr float GlideSeconds = 0.4f;
// A finger lifting slower than this (dp a second) leaves the list where it is.
constexpr float MinGlideSpeedDp = 150.f;
// A glide this close to its end (px) has ended.
constexpr float GlideEndedDistance = 1.f;
// The cells of an item grid (UI/Inventory): a finger dragged there carries the item.
constexpr const char* ItemGridClass = "item-grid";

bool IsScrollable(Rml::Style::Overflow overflow)
{
    return overflow == Rml::Style::Overflow::Auto || overflow == Rml::Style::Overflow::Scroll;
}

bool CanScrollX(Rml::Element* box)
{
    return IsScrollable(box->GetComputedValues().overflow_x()) && box->GetScrollWidth() > box->GetClientWidth();
}

bool CanScrollY(Rml::Element* box)
{
    return IsScrollable(box->GetComputedValues().overflow_y()) && box->GetScrollHeight() > box->GetClientHeight();
}

// Whether a drag starting on the element means something of its own: an item cell, a scrollbar, a
// slider's parts (RmlUi's drag property), a text field. `drag: block`, which base.rcss gives every
// clickable element, only keeps the window from moving with it: a list's rows scroll as on a phone.
bool KeepsDrag(const Rml::Element* element)
{
    const Rml::Style::Drag drag = element->GetComputedValues().drag();
    if (element->IsClassSet(ItemGridClass) || (drag != Rml::Style::Drag::None && drag != Rml::Style::Drag::Block))
        return true;
    const Rml::String& tag = element->GetTagName();
    return tag == "scrollbarvertical" || tag == "scrollbarhorizontal" || tag == "input" || tag == "textarea";
}

bool KeepsDragBetween(const Rml::Element* hit, const Rml::Element* box)
{
    for (const Rml::Element* element = hit; element != nullptr; element = element->GetParentNode())
    {
        if (KeepsDrag(element))
            return true;
        if (element == box)
            return false;
    }
    return false;
}

// A move between two window points in the box's own pixels: a scaled window moves its lists by the
// finger's distance on screen.
Rml::Vector2f LocalDelta(const Rml::Element* box, Rml::Vector2f from, Rml::Vector2f to)
{
    if (!box->Project(from) || !box->Project(to))
        return {};
    return to - from;
}

Rml::Vector2f ScrollOffset(Rml::Element* box)
{
    return {box->GetScrollLeft(), box->GetScrollTop()};
}

// The offsets `delta` away, kept inside the box's scroll range on the axes it scrolls.
Rml::Vector2f ScrolledBy(Rml::Element* box, Rml::Vector2f delta)
{
    Rml::Vector2f offset = ScrollOffset(box);
    if (CanScrollX(box))
        offset.x = std::clamp(offset.x + delta.x, 0.f, box->GetScrollWidth() - box->GetClientWidth());
    if (CanScrollY(box))
        offset.y = std::clamp(offset.y + delta.y, 0.f, box->GetScrollHeight() - box->GetClientHeight());
    return offset;
}
} // namespace

bool RmlTouchScroll::Begin(Rml::Context* context, Rml::Vector2f point)
{
    m_Held.reset();
    Rml::Element* hit = context != nullptr ? context->GetElementAtPoint(point) : nullptr;
    if (hit == nullptr)
        return false;
    Rml::Element* box = hit->GetClosestScrollableContainer();
    if (box == nullptr || (!CanScrollX(box) && !CanScrollY(box)) || KeepsDragBetween(hit, box))
        return false;
    m_Held = box->GetObserverPtr();
    return true;
}

void RmlTouchScroll::Move(Rml::Vector2f from, Rml::Vector2f to)
{
    Rml::Element* box = m_Held.get();
    if (box == nullptr)
        return;
    // The content follows the finger: a finger moving down shows what is above.
    const Rml::Vector2f offset = ScrolledBy(box, -LocalDelta(box, from, to));
    box->ScrollTo(offset);
}

void RmlTouchScroll::End(Rml::Vector2f point, Rml::Vector2f velocity)
{
    Rml::Element* box = m_Held.get();
    m_Held.reset();
    Rml::Context* context = box != nullptr ? box->GetContext() : nullptr;
    if (context == nullptr)
        return;
    const float minSpeed = MinGlideSpeedDp * context->GetDensityIndependentPixelRatio();
    if (velocity.SquaredMagnitude() < minSpeed * minSpeed)
        return;
    m_GlideTarget = ScrolledBy(box, -LocalDelta(box, point, point + velocity * GlideSeconds));
    box->ScrollTo(m_GlideTarget, Rml::ScrollBehavior::Smooth);
    m_Gliding = box->GetObserverPtr();
}

bool RmlTouchScroll::StopGlide()
{
    Rml::Element* box = m_Gliding.get();
    m_Gliding.reset();
    if (box == nullptr)
        return false;
    const Rml::Vector2f offset = ScrollOffset(box);
    if ((m_GlideTarget - offset).SquaredMagnitude() <= GlideEndedDistance * GlideEndedDistance)
        return false;
    // A smooth scroll by nothing ends the one under way (RmlUi's ScrollController).
    box->ScrollTo(offset, Rml::ScrollBehavior::Smooth);
    return true;
}

void RmlTouchScroll::Reset()
{
    m_Held.reset();
    m_Gliding.reset();
}
} // namespace Render::RmlUi
