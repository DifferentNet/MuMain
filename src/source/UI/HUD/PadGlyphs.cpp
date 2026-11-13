#include "stdafx.h"
#include "UI/HUD/PadGlyphs.h"

#include "Core/Input/GamepadInput.h"

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
    const Names& names = NamesOf(kind);
    SyncName(model.a, names.a, "glyph_a", markDirty);
    SyncName(model.b, names.b, "glyph_b", markDirty);
    SyncName(model.x, names.x, "glyph_x", markDirty);
    SyncName(model.y, names.y, "glyph_y", markDirty);
    SyncName(model.lb, names.lb, "glyph_lb", markDirty);
    SyncName(model.rb, names.rb, "glyph_rb", markDirty);
    SyncName(model.lt, names.lt, "glyph_lt", markDirty);
    SyncName(model.rt, names.rt, "glyph_rt", markDirty);
}
} // namespace UI::HUD::PadGlyphs
