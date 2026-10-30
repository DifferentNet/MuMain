#include "stdafx.h"
#include "UI/Mobile/PhoneHudLayout.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/HUD/TouchControls.h"
#include "UI/Mobile/NoticesUnderPanel.h"
#include "UI/Mobile/PhoneHudFit.h"
#include "UI/Mobile/TouchUnit.h"
#include "UI/Placement/WindowPlacement.h"
#include "UI/Scaling/UITransform.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <span>
#include <string>
#include <string_view>

namespace
{
using Edges = UI::Mobile::PhoneHudLayout::Edges;
using Clock = std::chrono::steady_clock;
namespace Fit = UI::Mobile::PhoneHudFit;

// Between the touch layer and a piece moved beside it, in finger units.
constexpr float GapUnits = 4.f;
// A piece whose document is not loaded is looked for again this often, not every frame.
constexpr auto SearchEvery = std::chrono::milliseconds(250);
// The pieces are placed again when the touch layer, the screen or a piece's own size changed, and
// this often anyway: a change nothing here watches (a containing block's transform after a UI
// scale change) is followed within it.
constexpr auto PlaceAgainEvery = std::chrono::milliseconds(250);
// Between two columns of the party's cards, in the cards' reference px: the panel is that much wider
// than its columns and spreads them apart (align-content: space-between, party_list.rcss).
constexpr float CardColumnGap = 2.f;
// A column's height holds exactly its cards, whatever their sizes round to.
constexpr float ColumnSlack = 0.5f;

// What the pieces are placed from, besides their own sizes.
struct Frame
{
    Edges edges;
    float gap = 0.f;
    Rml::Vector2i screen;
    float worldRight = 0.f;
};

// A piece: its document's file (found as WindowPlacement.cpp finds documents), its element, how it
// is placed, the properties that places it (Release() takes them off again), and what else its place
// depends on besides the frame and its own size (0 when nothing).
struct Piece
{
    std::string_view document;
    const char* selector;
    void (*place)(Rml::Element& element, const Frame& frame);
    std::span<const char* const> moved;
    float (*own)(Rml::Element& element);
};

// Where a piece was found, and what it was last placed from.
struct PieceState
{
    Rml::ObserverPtr<Rml::Element> element;
    Clock::time_point searchedAt{};
    const Rml::Element* placedElement = nullptr;
    Rml::Vector2f placedSize;
    float placedOwn = 0.f;
};

bool g_following = false;
Frame g_frame;
Clock::time_point g_placedAt{};

// Where the HUD lies over the world labels: the top row's band, the hero frame, the controls.
constexpr std::size_t LabelZoneRoom = Edges::MaxControls + 2;
std::array<UI::Mobile::TouchTarget::Box, LabelZoneRoom> g_labelZones{};
std::size_t g_labelZoneCount = 0;

void ReadLabelZones(const Frame& frame)
{
    const Edges& edges = frame.edges;
    std::size_t count = 0;
    // The whole band: the page's own menu button lies right of the row.
    g_labelZones[count++] = {true, 0.f, 0.f, static_cast<float>(frame.screen.x), edges.topRowBottom};
    g_labelZones[count++] = {true, edges.heroLeft, 0.f, edges.heroRight, edges.heroBottom};
    for (const UI::Mobile::TouchTarget::Box& control : edges.Controls())
        g_labelZones[count++] = control;
    g_labelZoneCount = count;
}

bool SameBox(const UI::Mobile::TouchTarget::Box& a, const UI::Mobile::TouchTarget::Box& b)
{
    return a.shown == b.shown && a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool SameEdges(const Edges& a, const Edges& b)
{
    const bool lines = a.topRowBottom == b.topRowBottom && a.topRowRight == b.topRowRight &&
                       a.heroLeft == b.heroLeft && a.heroRight == b.heroRight && a.heroBottom == b.heroBottom &&
                       a.potionsTop == b.potionsTop && a.potionsRight == b.potionsRight &&
                       a.skillsLeft == b.skillsLeft;
    return lines && std::ranges::equal(a.Controls(), b.Controls(), SameBox);
}

bool SameFrame(const Frame& a, const Frame& b)
{
    return a.gap == b.gap && a.screen == b.screen && a.worldRight == b.worldRight && SameEdges(a.edges, b.edges);
}

bool IsFile(std::string_view url, std::string_view file)
{
    return url.size() > file.size() && url.ends_with(file) && url[url.size() - file.size() - 1] == '/';
}

Rml::Element* Search(const Piece& piece)
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (context == nullptr)
        return nullptr;
    for (int i = 0; i < context->GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context->GetDocument(i);
        if (!IsFile(document->GetSourceURL(), piece.document))
            continue;
        if (Rml::Element* element = document->QuerySelector(piece.selector))
            return element;
    }
    return nullptr;
}

// The piece's element, kept while it lives (a theme switch builds the documents anew).
Rml::Element* Find(const Piece& piece, PieceState& state, Clock::time_point now)
{
    if (Rml::Element* element = state.element.get())
        return element;
    if (now - state.searchedAt < SearchEvery)
        return nullptr;
    state.searchedAt = now;
    Rml::Element* element = Search(piece);
    if (element == nullptr)
        return nullptr;
    state.element = element->GetObserverPtr();
    // A new element has none of the places set on the old one.
    state.placedElement = nullptr;
    return element;
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

// Sets a length when it differs, as each set lays the document out again. True when set.
bool SetLength(Rml::Element& element, Rml::PropertyId id, float px)
{
    const Rml::Property* current = element.GetLocalProperty(id);
    if (current != nullptr && current->unit == Rml::Unit::PX && current->Get<float>() == px)
        return false;
    element.SetProperty(id, Rml::Property(px, Rml::Unit::PX));
    return true;
}

// The same in whole pixels.
bool SetPx(Rml::Element& element, Rml::PropertyId id, float px)
{
    return SetLength(element, id, std::round(px));
}

bool SetCorner(Rml::Element& element, Rml::Vector2f point)
{
    const Rml::Vector2f offset = OffsetFor(element, point);
    const bool left = SetPx(element, Rml::PropertyId::Left, offset.x);
    const bool top = SetPx(element, Rml::PropertyId::Top, offset.y);
    return left || top;
}

void PlaceBuffs(Rml::Element& buffs, const Frame& frame)
{
    // Under the top row; the theme centres them across.
    SetPx(buffs, Rml::PropertyId::Top, OffsetFor(buffs, {0.f, frame.edges.topRowBottom + frame.gap}).y);
}

void PlacePets(Rml::Element& pets, const Frame& frame)
{
    SetCorner(pets, {frame.edges.heroRight + frame.gap, frame.edges.topRowBottom + frame.gap});
}

void PlaceSystemLog(Rml::Element& log, const Frame& frame)
{
    // Under the hero frame where its lines fit above the potions; on a short screen right of them.
    const Edges& edges = frame.edges;
    const float top = edges.heroBottom + frame.gap;
    const float height = log.GetBox().GetSize(Rml::BoxArea::Border).y;
    const float left = top + height + frame.gap <= edges.potionsTop ? edges.heroLeft : edges.potionsRight + frame.gap;
    // The windows in its slots follow the region.
    if (SetCorner(log, {left, top}))
        UI::Placement::Invalidate();
}

void PlaceSkillList(Rml::Element& list, const Frame& frame)
{
    // Its right edge just left of the skill buttons a skill picked in it goes on, but never past the
    // screen's left edge.
    const float width = list.GetBox().GetSize(Rml::BoxArea::Border).x;
    const float right = OffsetFor(list, {frame.edges.skillsLeft - frame.gap, 0.f}).x;
    const float leftmost = OffsetFor(list, {frame.gap, 0.f}).x;
    SetPx(list, Rml::PropertyId::Left, std::max(right - width, leftmost));
    SetPx(list, Rml::PropertyId::MarginLeft, 0.f);
}

void PlaceChat(Rml::Element& chat, const Frame& frame)
{
    // Where the theme put it: the margin box, as the margin set here moves the rest of it.
    const Rml::Vector2f themed = chat.GetAbsoluteOffset(Rml::BoxArea::Margin);
    const Rml::Vector2f size = chat.GetBox().GetSize(Rml::BoxArea::Border);
    if (size.x <= 0.f || size.y <= 0.f)
        return;
    // Left until it clears the skill buttons beside it, at most to the screen's edge. The chat lets
    // fingers through, so it may lie over the joystick's area.
    const float shift = Fit::ShiftLeft(frame.edges.Controls(), themed.x, themed.x + size.x, themed.y,
                                       themed.y + size.y, frame.gap, frame.gap);
    // The windows in its slots (the log, the input box) follow the region.
    if (SetPx(chat, Rml::PropertyId::MarginLeft, shift))
        UI::Placement::Invalidate();
}

// The party's shown cards: how many, and one's size in the panel's reference px.
struct PartyCards
{
    int count = 0;
    Rml::Vector2f size;
    float pitch = 0.f;
};

PartyCards ReadCards(Rml::Element& panel)
{
    PartyCards cards;
    for (int i = 0; i < panel.GetNumChildren(); ++i)
    {
        // The list's template card is kept hidden among them.
        Rml::Element* card = panel.GetChild(i);
        if (!card->IsClassSet("card") || !card->IsVisible())
            continue;
        if (cards.count++ == 0)
        {
            cards.size = card->GetBox().GetSize(Rml::BoxArea::Border);
            cards.pitch = cards.size.y + card->GetBox().GetEdge(Rml::BoxArea::Margin, Rml::BoxEdge::Bottom);
        }
    }
    return cards;
}

float ShownCards(Rml::Element& panel)
{
    return static_cast<float>(ReadCards(panel).count);
}

// The scale last set on the party's panel: it is only written (formatted) again when it changes.
struct CardScale
{
    const Rml::Element* panel = nullptr;
    float scale = 0.f;
};
CardScale g_cardScale;

// The cards' scale: the dock's, as the theme draws them, times the fit's. --root-scale is the
// panel's scale for its names' sharp text (base.rcss).
void SetCardScale(Rml::Element& panel, float scale)
{
    // A panel built anew has no transform of its own yet, even where the old one stood.
    const bool fresh = panel.GetLocalProperty(Rml::PropertyId::Transform) == nullptr;
    if (!fresh && g_cardScale.panel == &panel && g_cardScale.scale == scale)
        return;
    g_cardScale = {&panel, scale};
    const std::string value = std::to_string(scale);
    panel.SetProperty("transform", "scale(" + value + ")");
    panel.SetProperty("--root-scale", value);
}

// In one or two columns under the top row's right end, the first column on the right (the panel
// wraps its cards in reverse, party_list.rcss), above the controls round Attack: as big as fits. The
// panel's height ends a column after `rows` cards.
void PlaceParty(Rml::Element& panel, const Frame& frame)
{
    const Edges& edges = frame.edges;
    const PartyCards cards = ReadCards(panel);
    if (edges.topRowRight <= 0.f || cards.count == 0 || cards.pitch <= 0.f || !panel.GetOwnerDocument()->IsVisible())
        return;
    const float dock = UI::Scaling::DockRightTransform(frame.screen.x, frame.screen.y).scaleX;

    Fit::PartyArea area;
    area.right = std::min(edges.topRowRight, frame.worldRight - frame.gap);
    area.top = edges.topRowBottom + frame.gap;
    area.left = edges.heroRight + frame.gap;
    area.floor = static_cast<float>(frame.screen.y);
    area.gap = frame.gap;
    area.cardWidth = cards.size.x * dock;
    area.cardHeight = cards.size.y * dock;
    area.cardPitch = cards.pitch * dock;
    area.columnGap = CardColumnGap * dock;
    const Fit::PartyFit fit = Fit::FitParty(area, cards.count, edges.Controls());

    const float scale = dock * fit.scale;
    const float width = static_cast<float>(fit.columns) * (cards.size.x + CardColumnGap) - CardColumnGap;
    const float height = static_cast<float>(fit.rows) * cards.pitch + ColumnSlack;
    SetCorner(panel, {area.right - width * scale, area.top});
    // Lengths inside the panel are its reference px, under its scale.
    SetLength(panel, Rml::PropertyId::Width, width);
    SetLength(panel, Rml::PropertyId::Height, height);
    SetCardScale(panel, scale);
}

constexpr const char* CornerMoved[] = {"left", "top", "margin-left"};
constexpr const char* PartyMoved[] = {"left", "top", "width", "height", "transform", "--root-scale"};
constexpr const char* ChatMoved[] = {"margin-left"};

constexpr Piece Pieces[] = {
    {"buff_strip.rml", "#panel", PlaceBuffs, CornerMoved, nullptr},
    {"item_endurance.rml", "#pets", PlacePets, CornerMoved, nullptr},
    {"workspace.rml", "#system_log_stack", PlaceSystemLog, CornerMoved, nullptr},
    {"main_frame.rml", "#skill_list", PlaceSkillList, CornerMoved, nullptr},
    {"party_list.rml", "#panel", PlaceParty, PartyMoved, ShownCards},
    {"workspace.rml", ".chat-stack", PlaceChat, ChatMoved, nullptr},
};
constexpr std::size_t PieceCount = std::size(Pieces);

std::array<PieceState, PieceCount> g_states;

Frame ReadFrame(const Edges& edges)
{
    Frame frame;
    frame.edges = edges;
    frame.gap = GapUnits * UI::Mobile::TouchUnit::Pixels();
    if (const Rml::Context* context = RmlUiRuntime::Instance().GetContext())
        frame.screen = context->GetDimensions();
    frame.worldRight = UI::Placement::UncoveredWorldRight();
    return frame;
}

void FollowPiece(const Piece& piece, PieceState& state, bool again, Clock::time_point now)
{
    Rml::Element* element = Find(piece, state, now);
    if (element == nullptr)
        return;
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
    const float own = piece.own != nullptr ? piece.own(*element) : 0.f;
    if (!again && element == state.placedElement && size == state.placedSize && own == state.placedOwn)
        return;
    state.placedElement = element;
    state.placedSize = size;
    state.placedOwn = own;
    piece.place(*element, g_frame);
}
} // namespace

void UI::Mobile::PhoneHudLayout::Follow(const Edges& edges)
{
    const Frame frame = ReadFrame(edges);
    const Clock::time_point now = Clock::now();
    const bool again = !g_following || !SameFrame(frame, g_frame) || now - g_placedAt >= PlaceAgainEvery;
    g_following = true;
    if (again)
    {
        g_frame = frame;
        g_placedAt = now;
        ReadLabelZones(frame);
    }
    for (std::size_t i = 0; i < PieceCount; ++i)
        FollowPiece(Pieces[i], g_states[i], again, now);
    UI::Mobile::NoticesUnderPanel::Sync(UI::HUD::TouchControls::IsMenuOpen());
}

void UI::Mobile::PhoneHudLayout::Release()
{
    UI::Mobile::NoticesUnderPanel::Release();
    if (!g_following)
        return;
    g_following = false;
    g_cardScale = {};
    g_labelZoneCount = 0;
    for (std::size_t i = 0; i < PieceCount; ++i)
    {
        PieceState& state = g_states[i];
        state.placedElement = nullptr;
        Rml::Element* element = state.element.get();
        if (element == nullptr)
            continue;
        for (const char* name : Pieces[i].moved)
            element->RemoveProperty(name);
    }
    UI::Placement::Invalidate();
}

bool UI::Mobile::PhoneHudLayout::CoversLabel(const TouchTarget::Box& label)
{
    return Fit::Overlaps({g_labelZones.data(), g_labelZoneCount}, label);
}
