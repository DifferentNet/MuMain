#include "stdafx.h"
#include "UI/HUD/PadTargets.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/Placement/WindowPlacement.h"
#include "UI/RmlBridge/RmlStackingOrder.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace UI::HUD::PadTargets
{
namespace
{
using PadTargetPick::Box;
using PadTargetPick::Target;

// The documents over the windows that are not windows themselves: the HUD's bar, the pad's own frame,
// the tooltips.
constexpr std::array<std::string_view, 5> NotWindows = {"main_frame.rml", "pad_overlay.rml", "tooltip.rml",
                                                        "cursor_item.rml", "item_explanation.rml"};
// Documents drawn this deep or deeper, up to the message boxes, are screens and dialogs over the
// windows (the help, the move list, the options, the message boxes) even without a workspace slot.
constexpr float FloatingDepth = 8.f;
constexpr float MessageBoxDepth = 10.7f;
// The roots a window the workspace places carries the class "slot-placed" on.
constexpr std::array<const char*, 2> PlacedRoots = {"panel", "content_root"};
constexpr const char* PlacedClass = "slot-placed";
constexpr const char* CellClass = "item-cell";
constexpr const char* SlotClass = "equip-slot";
// A cell under an item carries its tint (ItemGridCell::tint), an empty one none.
constexpr std::string_view TintPrefix = "tint-";
// A worn slot with nothing in it (MyInventory's slot_states).
constexpr const char* BareSlotClass = "bare";
// A grid whose items are only to look at (the partner's offer in a trade): its tooltip shows them,
// the pad offers nothing to do there.
constexpr const char* LookOnlySelector = ".look-only";
constexpr std::array<const char*, 2> ClickAttributes = {"data-event-click", "onclick"};
// The cross in a window's corner: B closes the window already, the frame does not stop on it.
constexpr const char* CornerCloseId = "frame_corner_close";
// A box this much of the screen or more is a panel, not a control.
constexpr float LargestShare = 0.25f;
// A target in a list less than this share on view is left out: the list scrolls to it first.
constexpr float LeastShown = 0.5f;
// A window's lists the d-pad can scroll; more are left as they are.
constexpr int MostLists = 32;

std::string_view FileName(const Rml::String& url)
{
    const std::string_view path(url);
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

bool IsPlaced(Rml::ElementDocument& document)
{
    for (const char* rootId : PlacedRoots)
    {
        const Rml::Element* root = document.GetElementById(rootId);
        if (root != nullptr && root->IsClassSet(PlacedClass))
            return true;
    }
    return false;
}

// The box of a window's root (#panel or #content_root), false for a document without one.
bool PlacedBox(Rml::ElementDocument& document, Box& box);

// The message boxes and the menus over the windows (the item card): while one is up, the frame
// rests only on it, as the windows under it take no clicks.
bool IsDialog(Rml::ElementDocument& document)
{
    const std::optional<float> depth =
        UI::RmlBridge::StackingDepthForDocument(FileName(document.GetSourceURL()));
    return depth && *depth >= MessageBoxDepth;
}

bool IsWindow(Rml::ElementDocument& document)
{
    if (!document.IsVisible())
        return false;
    const std::string_view name = FileName(document.GetSourceURL());
    for (const std::string_view excluded : NotWindows)
    {
        if (name == excluded)
            return false;
    }
    if (IsPlaced(document) || UI::Placement::IsWindowDocument(name))
        return true;
    const std::optional<float> depth = UI::RmlBridge::StackingDepthForDocument(name);
    return depth && *depth >= FloatingDepth && *depth <= MessageBoxDepth;
}

bool IsTarget(Rml::Element& element)
{
    if (element.GetId() == CornerCloseId)
        return false;
    if (element.IsClassSet(CellClass) || element.IsClassSet(SlotClass))
        return true;
    const Rml::String& tag = element.GetTagName();
    if (tag == "button" || tag == "input" || tag == "select")
        return true;
    // Many windows make their buttons from divs that only bind a click (the stat "+", the window buttons).
    for (const char* click : ClickAttributes)
    {
        if (element.HasAttribute(click))
            return true;
    }
    return element.GetComputedValues().drag() == Rml::Style::Drag::Block;
}

bool DrawnBox(Rml::Element& element, Box& box)
{
    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (!UI::RmlBridge::DrawnBox(element, Rml::BoxArea::Border, offset, size))
        return false;
    box = {offset.x, offset.y, offset.x + size.x, offset.y + size.y};
    return true;
}

// The list that scrolls the element: its nearest ancestor with a vertical scroll.
Rml::Element* ScrollingList(Rml::Element& element)
{
    for (Rml::Element* ancestor = element.GetParentNode(); ancestor != nullptr; ancestor = ancestor->GetParentNode())
    {
        const Rml::Style::Overflow overflow = ancestor->GetComputedValues().overflow_y();
        const bool scrolls = overflow == Rml::Style::Overflow::Auto || overflow == Rml::Style::Overflow::Scroll;
        if (scrolls && ancestor->GetScrollHeight() > ancestor->GetClientHeight())
            return ancestor;
    }
    return nullptr;
}

float Area(const Box& box)
{
    return std::max(box.right - box.left, 0.f) * std::max(box.bottom - box.top, 0.f);
}

Box Overlap(const Box& a, const Box& b)
{
    return {std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right), std::min(a.bottom, b.bottom)};
}

class Collector
{
public:
    void Begin(Rml::Context& context)
    {
        m_Targets.clear();
        m_ListCount = 0;
        m_WindowsLeft = std::numeric_limits<float>::max();
        const Rml::Vector2i size = context.GetDimensions();
        m_Width = static_cast<float>(size.x);
        m_LargestArea = static_cast<float>(size.x) * static_cast<float>(size.y) * LargestShare;
    }

    void AddDocument(Rml::ElementDocument& document, int window)
    {
        m_Window = window;
        Walk(document);
    }

    // The windows docked on the right (the game's own column), not the screens centred over them.
    void NoteWindow(Rml::ElementDocument& document)
    {
        if (!IsPlaced(document) && !UI::Placement::IsWindowDocument(FileName(document.GetSourceURL())))
            return;
        Box placed;
        if (PlacedBox(document, placed) && Area(placed) > 0.f && placed.left >= m_Width * 0.5f)
            m_WindowsLeft = std::min(m_WindowsLeft, placed.left);
    }

    float WindowsLeft() const { return m_WindowsLeft; }

    std::span<const Target> Targets() const { return m_Targets; }

private:
    void Walk(Rml::Element& element)
    {
        for (int i = 0; i < element.GetNumChildren(); ++i)
        {
            Rml::Element* child = element.GetChild(i);
            if (child == nullptr || !child->IsVisible())
                continue;
            if (IsTarget(*child))
                Add(*child);
            Walk(*child);
        }
    }

    void Add(Rml::Element& element)
    {
        Target target{{}, m_Window};
        target.cell = element.IsClassSet(CellClass);
        target.slot = element.IsClassSet(SlotClass);
        target.empty = (target.cell && element.GetClassNames().find(TintPrefix) == Rml::String::npos)
                       || (target.slot && element.IsClassSet(BareSlotClass))
                       || (target.cell && element.Closest(LookOnlySelector) != nullptr);
        if (!DrawnBox(element, target.box) || Area(target.box) <= 0.f || Area(target.box) >= m_LargestArea)
            return;
        Rml::Element* list = ScrollingList(element);
        if (list != nullptr && !PlaceInList(*list, target))
            return;
        m_Targets.push_back(target);
    }

    // Marks the target with its list, false when the list shows too little of it.
    bool PlaceInList(Rml::Element& list, Target& target)
    {
        Box shown;
        if (!DrawnBox(list, shown) || Area(Overlap(target.box, shown)) < Area(target.box) * LeastShown)
            return false;
        target.list = ListIndex(list);
        target.listScrollsUp = list.GetScrollTop() > 0.f;
        target.listScrollsDown = list.GetScrollTop() + list.GetClientHeight() < list.GetScrollHeight() - 1.f;
        return true;
    }

    int ListIndex(const Rml::Element& list)
    {
        for (int i = 0; i < m_ListCount; ++i)
        {
            if (m_Lists[i] == &list)
                return i;
        }
        if (m_ListCount == MostLists)
            return PadTargetPick::None;
        m_Lists[m_ListCount] = &list;
        return m_ListCount++;
    }

    std::vector<Target> m_Targets;
    std::array<const Rml::Element*, MostLists> m_Lists{};
    int m_ListCount = 0;
    int m_Window = 0;
    float m_LargestArea = 0.f;
    float m_WindowsLeft = std::numeric_limits<float>::max();
    float m_Width = 0.f;
};

Collector g_collector;
bool g_onDialog = false;

bool PlacedBox(Rml::ElementDocument& document, Box& box)
{
    for (const char* rootId : PlacedRoots)
    {
        Rml::Element* root = document.GetElementById(rootId);
        if (root != nullptr)
            return DrawnBox(*root, box);
    }
    return false;
}
} // namespace

std::span<const PadTargetPick::Target> Collect()
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (context == nullptr)
        return {};
    g_collector.Begin(*context);
    for (int i = 0; i < context->GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context->GetDocument(i);
        if (document == nullptr || !IsWindow(*document))
            continue;
        g_collector.NoteWindow(*document);
        if (IsDialog(*document))
            g_collector.AddDocument(*document, i);
    }
    g_onDialog = !g_collector.Targets().empty();
    if (g_onDialog)
        return g_collector.Targets();
    for (int i = 0; i < context->GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context->GetDocument(i);
        if (document != nullptr && IsWindow(*document))
            g_collector.AddDocument(*document, i);
    }
    return g_collector.Targets();
}

bool OnDialog()
{
    return g_onDialog;
}

std::optional<float> WindowsLeft()
{
    const float left = g_collector.WindowsLeft();
    if (left == std::numeric_limits<float>::max())
        return std::nullopt;
    return left;
}

bool IsIn(const PadTargetPick::Target& target, std::string_view fileName)
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (context == nullptr || target.window < 0 || target.window >= context->GetNumDocuments())
        return false;
    Rml::ElementDocument* document = context->GetDocument(target.window);
    return document != nullptr && FileName(document->GetSourceURL()) == fileName;
}
} // namespace UI::HUD::PadTargets
