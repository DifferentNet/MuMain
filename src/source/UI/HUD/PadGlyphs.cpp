#include "stdafx.h"
#include "UI/HUD/PadGlyphs.h"

#include "Core/Input/GamepadInput.h"
#include "Core/Input/GamepadLayout.h"
#include "I18N/All.h"

#include <utility>

namespace UI::HUD::PadGlyphs
{
namespace
{
struct Names
{
    const char* a;
    const char* b;
    const char* x;
    const char* y;
    const char* lb;
    const char* rb;
    const char* lt;
    const char* rt;
};

// By place: south, east, west, north, then the shoulders and the triggers.
constexpr Names XboxNames{"A", "B", "X", "Y", "LB", "RB", "LT", "RT"};
constexpr Names PlayStationNames{"", "", "", "", "L1", "R1", "L2", "R2"};
constexpr Names NintendoNames{"B", "A", "Y", "X", "L", "R", "ZL", "ZR"};

const Names& NamesOf(Core::Input::Gamepad::PadKind kind)
{
    switch (kind)
    {
    case Core::Input::Gamepad::PadKind::PlayStation:
        return PlayStationNames;
    case Core::Input::Gamepad::PadKind::Nintendo:
        return NintendoNames;
    case Core::Input::Gamepad::PadKind::Xbox:
        break;
    }
    return XboxNames;
}

void SyncName(Rml::String& field, const char* value, const char* name,
              const std::function<void(const char*)>& markDirty)
{
    if (field == value)
        return;
    field = value;
    markDirty(name);
}

// The sticks' glyphs are the same for every maker: LS, RS, and L3 for the left stick pressed in.
constexpr const char* LeftStickName = "LS";
constexpr const char* RightStickName = "RS";
constexpr const char* LeftClickName = "L3";
constexpr const char* RightClickName = "R3";

// The names by the game's button each physical one plays: the layout's pairs trade their names.
void SwapForLayout(Names& names, Core::Input::Gamepad::Layout layout)
{
    if (!Core::Input::Gamepad::SwapsShoulders(layout))
        return;
    std::swap(names.lb, names.lt);
    std::swap(names.rb, names.rt);
}

void SwapForLayout(PadButtonNames::Names& names, Core::Input::Gamepad::Layout layout)
{
    if (Core::Input::Gamepad::SwapsShoulders(layout))
    {
        std::swap(names.lb, names.lt);
        std::swap(names.rb, names.rt);
    }
    if (Core::Input::Gamepad::SwapsSticks(layout))
    {
        std::swap(names.ls, names.rs);
        std::swap(names.walkStick, names.aimStick);
        std::swap(names.walkStickObject, names.aimStickObject);
    }
}
} // namespace

void Bind(Rml::DataModelConstructor& constructor, Model& model)
{
    constructor.Bind("glyph_family", &model.family);
    constructor.Bind("glyph_a", &model.a);
    constructor.Bind("glyph_b", &model.b);
    constructor.Bind("glyph_x", &model.x);
    constructor.Bind("glyph_y", &model.y);
    constructor.Bind("glyph_lb", &model.lb);
    constructor.Bind("glyph_rb", &model.rb);
    constructor.Bind("glyph_lt", &model.lt);
    constructor.Bind("glyph_rt", &model.rt);
    constructor.Bind("glyph_ls", &model.ls);
    constructor.Bind("glyph_rs", &model.rs);
    constructor.Bind("glyph_l3", &model.l3);
    constructor.Bind("glyph_swap", &model.swapShoulders);
}

void Sync(Model& model, const std::function<void(const char*)>& markDirty)
{
    const Core::Input::Gamepad::PadKind kind = Core::Input::Gamepad::ShownKind();
    const int family = static_cast<int>(kind);
    if (model.family != family)
    {
        model.family = family;
        markDirty("glyph_family");
    }
    const Core::Input::Gamepad::Layout layout = Core::Input::Gamepad::GetLayout();
    Names names = NamesOf(kind);
    SwapForLayout(names, layout);
    const bool sticksSwapped = Core::Input::Gamepad::SwapsSticks(layout);
    const bool shouldersSwapped = Core::Input::Gamepad::SwapsShoulders(layout);
    if (model.swapShoulders != shouldersSwapped)
    {
        model.swapShoulders = shouldersSwapped;
        markDirty("glyph_swap");
    }
    SyncName(model.a, names.a, "glyph_a", markDirty);
    SyncName(model.b, names.b, "glyph_b", markDirty);
    SyncName(model.x, names.x, "glyph_x", markDirty);
    SyncName(model.y, names.y, "glyph_y", markDirty);
    SyncName(model.lb, names.lb, "glyph_lb", markDirty);
    SyncName(model.rb, names.rb, "glyph_rb", markDirty);
    SyncName(model.lt, names.lt, "glyph_lt", markDirty);
    SyncName(model.rt, names.rt, "glyph_rt", markDirty);
    SyncName(model.ls, sticksSwapped ? RightStickName : LeftStickName, "glyph_ls", markDirty);
    SyncName(model.rs, sticksSwapped ? LeftStickName : RightStickName, "glyph_rs", markDirty);
    SyncName(model.l3, sticksSwapped ? RightClickName : LeftClickName, "glyph_l3", markDirty);
}

PadButtonNames::Names TextNames(Core::Input::Gamepad::PadKind kind)
{
    PadButtonNames::Names names;
    // The sticks in the language's words, left walking and right aiming until the layout swaps them.
    names.walkStick = I18N::Game::PadStickLeft;
    names.aimStick = I18N::Game::PadStickRight;
    names.walkStickObject = I18N::Game::PadStickLeftObject;
    names.aimStickObject = I18N::Game::PadStickRightObject;
    switch (kind)
    {
    case Core::Input::Gamepad::PadKind::PlayStation:
        names.a = I18N::Game::PadNameCross;
        names.b = I18N::Game::PadNameCircle;
        names.x = I18N::Game::PadNameSquare;
        names.y = I18N::Game::PadNameTriangle;
        names.lb = L"L1";
        names.rb = L"R1";
        names.lt = L"L2";
        names.rt = L"R2";
        names.ls = L"L3";
        names.rs = L"R3";
        names.menu = L"Options";
        names.view = L"Create";
        break;
    case Core::Input::Gamepad::PadKind::Nintendo:
        names.a = L"B";
        names.b = L"A";
        names.x = L"Y";
        names.y = L"X";
        names.lb = L"L";
        names.rb = L"R";
        names.lt = L"ZL";
        names.rt = L"ZR";
        names.menu = L"+";
        names.view = L"-";
        break;
    case Core::Input::Gamepad::PadKind::Xbox:
        break;
    }
    SwapForLayout(names, Core::Input::Gamepad::GetLayout());
    return names;
}

std::wstring Named(const wchar_t* text)
{
    return PadButtonNames::Fill(text != nullptr ? text : L"", TextNames(Core::Input::Gamepad::ShownKind()));
}
} // namespace UI::HUD::PadGlyphs
