#include "stdafx.h"
#include "UI/Mobile/PhoneHudLayout.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Mobile/TouchUnit.h"
#include "UI/Placement/WindowPlacement.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace
{
using Edges = UI::Mobile::PhoneHudLayout::Edges;

// Between the touch layer and a piece moved beside it, in finger units.
constexpr float GapUnits = 4.f;

// A piece: its document's file (found as WindowPlacement.cpp finds documents) and its element's id.
struct Piece
{
    std::string_view document;
    const char* id;
};
constexpr Piece Buffs{"buff_strip.rml", "panel"};
constexpr Piece Pets{"item_endurance.rml", "pets"};
constexpr Piece SystemLog{"workspace.rml", "system_log_stack"};
constexpr Piece SkillList{"main_frame.rml", "skill_list"};
constexpr Piece Pieces[] = {Buffs, Pets, SystemLog, SkillList};

// The properties Follow() sets on the pieces, which Release() takes off again.
constexpr Rml::PropertyId MovedProperties[] = {Rml::PropertyId::Left, Rml::PropertyId::Top,
                                               Rml::PropertyId::MarginLeft};

bool g_following = false;

bool IsFile(std::string_view url, std::string_view file)
{
    return url.size() > file.size() && url.ends_with(file) && url[url.size() - file.size() - 1] == '/';
}

Rml::Element* Find(const Piece& piece)
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (context == nullptr)
        return nullptr;
    for (int i = 0; i < context->GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context->GetDocument(i);
        if (!IsFile(document->GetSourceURL(), piece.document))
            continue;
        if (Rml::Element* element = document->GetElementById(piece.id))
            return element;
    }
    return nullptr;
}

// What an element's left and top must be for its corner to lie on a window point: the point in its
// containing block's layout, through any transform on the block or above it (the pets' column is drawn
// in the screen's stretch, the HUD's bar at its own scale).
Rml::Vector2f OffsetFor(Rml::Element& element, Rml::Vector2f point)
{
    Rml::Element* block = element.GetOffsetParent();
    if (block == nullptr)
        return point;
    block->Project(point);
    return point - block->GetAbsoluteOffset(Rml::BoxArea::Padding);
}

// Sets a length in whole pixels when it differs, as each set lays the document out again. True when set.
bool SetPx(Rml::Element& element, Rml::PropertyId id, float px)
{
    const float value = std::round(px);
    const Rml::Property* current = element.GetLocalProperty(id);
    if (current != nullptr && current->unit == Rml::Unit::PX && current->Get<float>() == value)
        return false;
    element.SetProperty(id, Rml::Property(value, Rml::Unit::PX));
    return true;
}

bool SetCorner(Rml::Element& element, Rml::Vector2f point)
{
    const Rml::Vector2f offset = OffsetFor(element, point);
    const bool left = SetPx(element, Rml::PropertyId::Left, offset.x);
    const bool top = SetPx(element, Rml::PropertyId::Top, offset.y);
    return left || top;
}

void PlaceBuffs(const Edges& edges, float gap)
{
    // Under the top row; the theme centres them across.
    if (Rml::Element* buffs = Find(Buffs))
        SetPx(*buffs, Rml::PropertyId::Top, OffsetFor(*buffs, {0.f, edges.topRowBottom + gap}).y);
}

void PlacePets(const Edges& edges, float gap)
{
    if (Rml::Element* pets = Find(Pets))
        SetCorner(*pets, {edges.heroRight + gap, edges.topRowBottom + gap});
}

void PlaceSystemLog(const Edges& edges, float gap)
{
    Rml::Element* log = Find(SystemLog);
    if (log == nullptr)
        return;
    // Under the hero frame where its lines fit above the potions; on a short screen right of them.
    const float top = edges.heroBottom + gap;
    const float height = log->GetBox().GetSize(Rml::BoxArea::Border).y;
    const float left = top + height + gap <= edges.potionsTop ? edges.heroLeft : edges.potionsRight + gap;
    // The windows in its slots follow the region.
    if (SetCorner(*log, {left, top}))
        UI::Placement::Invalidate();
}

void PlaceSkillList(const Edges& edges, float gap)
{
    Rml::Element* list = Find(SkillList);
    if (list == nullptr)
        return;
    // Its right edge just left of the skill buttons a skill picked in it goes on, but never past the
    // screen's left edge.
    const float width = list->GetBox().GetSize(Rml::BoxArea::Border).x;
    const float right = OffsetFor(*list, {edges.skillsLeft - gap, 0.f}).x;
    const float leftmost = OffsetFor(*list, {gap, 0.f}).x;
    SetPx(*list, Rml::PropertyId::Left, std::max(right - width, leftmost));
    SetPx(*list, Rml::PropertyId::MarginLeft, 0.f);
}
} // namespace

void UI::Mobile::PhoneHudLayout::Follow(const Edges& edges)
{
    g_following = true;
    const float gap = GapUnits * TouchUnit::Pixels();
    PlaceBuffs(edges, gap);
    PlacePets(edges, gap);
    PlaceSystemLog(edges, gap);
    PlaceSkillList(edges, gap);
}

void UI::Mobile::PhoneHudLayout::Release()
{
    if (!g_following)
        return;
    g_following = false;
    for (const Piece& piece : Pieces)
    {
        Rml::Element* element = Find(piece);
        if (element == nullptr)
            continue;
        for (const Rml::PropertyId id : MovedProperties)
            element->RemoveProperty(id);
    }
    UI::Placement::Invalidate();
}
