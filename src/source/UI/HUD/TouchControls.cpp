#include "stdafx.h"
#include "UI/HUD/TouchControls.h"

#include "Audio/DSPlaySound.h"
#include "Camera/CameraManager.h"
#include "Core/Input/TouchInput.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Touch/TouchActions.h"
#include "I18N/All.h"
#include "MUHelper/MuHelper.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/SkillList.h"
#include "UI/Mobile/HeroFrame.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace
{
namespace Touch = Core::Input::Touch;

constexpr std::string_view DocumentName = "touch_controls.rml";
constexpr int SkillButtons = 5;

// Buttons that press a key, as a keyboard would: the potions drink the item hotkeys Q and W, the chat
// button opens the chat field (Enter), Back closes the window on top or, with none open, opens the
// system menu (Esc, as a phone has none), and the rest open their windows (UI/HUD/HotKey.cpp).
struct KeyButton
{
    const char* element;
    int key;
};

constexpr KeyButton KeyButtons[] = {
    {"tc_potion_q", 'Q'}, {"tc_potion_w", 'W'}, {"tc_chat", VK_RETURN}, {"tc_inventory", 'I'},
    {"tc_character", 'C'}, {"tc_move", 'M'},     {"tc_map", VK_TAB},       {"tc_back", VK_ESCAPE},
};
constexpr int KeyButtonCount = static_cast<int>(std::size(KeyButtons));

// The skill list, as the PC bar's current-skill icon opens it (the phone layout hides that bar).
void ToggleSkillList()
{
    g_pSkillList->OnCurrentSkillClick();
}

// Between the phone and the PC layout, saved as a choice of its own (Options -> Interface shows it).
void ToggleView()
{
    using UI::Mobile::ViewChoice;
    UI::Mobile::SetViewChoice(UI::Mobile::IsPhoneView() ? ViewChoice::Pc : ViewChoice::Phone);
}

// The menu's items: windows by the key that opens them, or an action of their own. Their labels are
// menu_label_0.. in the markup.
struct MenuItem
{
    const wchar_t* const* label;
    // Pressed as a keyboard would, when the item has no action.
    int key;
    void (*action)() = nullptr;
};

const MenuItem MenuItems[] = {
    {&I18N::Game::TouchMenuInventory, 'I'},
    {&I18N::Game::TouchMenuCharacter, 'C'},
    {&I18N::Game::TouchMenuParty, 'P'},
    {&I18N::Game::TouchMenuGuild, 'G'},
    {&I18N::Game::TouchMenuFriends, 'F'},
    {&I18N::Game::TouchMenuQuests, 'T'},
    {&I18N::Game::TouchMenuMap, VK_TAB},
    {&I18N::Game::TouchMenuMove, 'M'},
    {&I18N::Game::TouchMenuBattlePass, 'X'},
    {&I18N::Game::TouchMenuDaily, 'N'},
    {&I18N::Game::TouchMenuEvents, 'O'},
    {&I18N::Game::TouchMenuHelper, 'Z'},
    {&I18N::Game::TouchMenuCommands, 'J'},
    {&I18N::Game::TouchMenuHelp, VK_F1},
    {&I18N::Game::TouchMenuSkills, 0, &ToggleSkillList},
    {&I18N::Game::TouchMenuView, 0, &ToggleView},
    {&I18N::Game::TouchMenuSystem, VK_ESCAPE},
};
constexpr int MenuItemCount = static_cast<int>(std::size(MenuItems));

// The controls' ids for Core::Input::Touch.
enum ControlId : int
{
    StickControl = 0,
    AttackControl,
    FirstSkillControl,
    PickupControl = FirstSkillControl + SkillButtons,
    AutoControl,
    MenuControl,
    FirstKeyControl,
    FirstMenuItemControl = FirstKeyControl + KeyButtonCount,
    ControlCount = FirstMenuItemControl + MenuItemCount,
};
static_assert(ControlCount <= Touch::MaxControls);

constexpr const char* SkillElements[SkillButtons] = {"tc_skill_0", "tc_skill_1", "tc_skill_2", "tc_skill_3",
                                                     "tc_skill_4"};

// Numbered names, "<prefix>0" .. "<prefix><MenuItemCount - 1>", made once.
std::array<std::string, MenuItemCount> NumberedNames(std::string_view prefix)
{
    std::array<std::string, MenuItemCount> names;
    for (int item = 0; item < MenuItemCount; ++item)
        names[item] = std::string(prefix) + std::to_string(item);
    return names;
}

// The menu items' elements in the markup, and their label fields in the model.
const std::array<std::string, MenuItemCount> MenuItemElements = NumberedNames("tc_menu_");
const std::array<std::string, MenuItemCount> MenuLabelFields = NumberedNames("menu_label_");

// Each control's element in touch_controls.rml.
const char* ControlElement(int control)
{
    switch (control)
    {
    case StickControl:
        return "tc_stick";
    case AttackControl:
        return "tc_attack";
    case PickupControl:
        return "tc_pickup";
    case AutoControl:
        return "tc_auto";
    case MenuControl:
        return "tc_menu";
    default:
        break;
    }
    if (control >= FirstSkillControl && control < FirstSkillControl + SkillButtons)
        return SkillElements[control - FirstSkillControl];
    if (control >= FirstKeyControl && control < FirstMenuItemControl)
        return KeyButtons[control - FirstKeyControl].element;
    return MenuItemElements[control - FirstMenuItemControl].c_str();
}

// How far outside its box a finger still presses a button, in dp: a thumb is wider than its aim.
constexpr float TouchSlopDp = 6.f;
// The knob goes no further from the stick's centre than this, in dp; a finger further out drags the
// centre along.
constexpr float StickReachDp = 48.f;
// A finger closer than this to the stick's centre, in dp, does not walk.
constexpr float StickDeadZoneDp = 10.f;

// Documents over the controls that still leave them the finger, as they only show text: the chat's
// log lies over the joystick's corner, the scrolling notice's band across the top row.
constexpr std::string_view PassThroughDocuments[] = {"chat_log.rml", "slide_notice.rml"};

// How long a skill picked in the skill list waits for a tap on a skill button to go on.
constexpr auto BindWindow = std::chrono::seconds(8);

struct ControlsModel
{
    bool stickActive = false;
    // Window pixels: the stick's centre where the finger landed, and the knob's offset from it.
    float stickX = 0.f;
    float stickY = 0.f;
    float knobX = 0.f;
    float knobY = 0.f;
    std::array<Rml::String, SkillButtons> skillIcons{"none", "none", "none", "none", "none"};
    std::array<float, SkillButtons> skillCooldowns{};
    std::array<bool, SkillButtons> skillSelected{};
    std::array<bool, SkillButtons> skillEmpty{true, true, true, true, true};
    // The buttons a skill picked in the skill list can go on: the one chosen, else all for a while.
    std::array<bool, SkillButtons> skillBind{};
    bool hintShown = false;
    Rml::String bindHint;
    bool helperOn = false;
    bool menuOpen = false;
    Rml::String attackLabel;
    Rml::String pickupLabel;
    Rml::String autoLabel;
    Rml::String menuLabel;
    Rml::String chatLabel;
    Rml::String backLabel;
    std::array<Rml::String, MenuItemCount> menuLabels;
    // The phone view's hero frame.
    UI::Mobile::HeroFrame::Model hero;
};

void BindModel(Rml::DataModelConstructor& c, ControlsModel& model)
{
    c.Bind("stick_active", &model.stickActive);
    c.Bind("stick_x", &model.stickX);
    c.Bind("stick_y", &model.stickY);
    c.Bind("knob_x", &model.knobX);
    c.Bind("knob_y", &model.knobY);
    c.Bind("skill_0_icon", &model.skillIcons[0]);
    c.Bind("skill_1_icon", &model.skillIcons[1]);
    c.Bind("skill_2_icon", &model.skillIcons[2]);
    c.Bind("skill_3_icon", &model.skillIcons[3]);
    c.Bind("skill_4_icon", &model.skillIcons[4]);
    c.Bind("skill_0_cooldown", &model.skillCooldowns[0]);
    c.Bind("skill_1_cooldown", &model.skillCooldowns[1]);
    c.Bind("skill_2_cooldown", &model.skillCooldowns[2]);
    c.Bind("skill_3_cooldown", &model.skillCooldowns[3]);
    c.Bind("skill_4_cooldown", &model.skillCooldowns[4]);
    c.Bind("skill_0_selected", &model.skillSelected[0]);
    c.Bind("skill_1_selected", &model.skillSelected[1]);
    c.Bind("skill_2_selected", &model.skillSelected[2]);
    c.Bind("skill_3_selected", &model.skillSelected[3]);
    c.Bind("skill_4_selected", &model.skillSelected[4]);
    c.Bind("skill_0_empty", &model.skillEmpty[0]);
    c.Bind("skill_1_empty", &model.skillEmpty[1]);
    c.Bind("skill_2_empty", &model.skillEmpty[2]);
    c.Bind("skill_3_empty", &model.skillEmpty[3]);
    c.Bind("skill_4_empty", &model.skillEmpty[4]);
    c.Bind("skill_0_bind", &model.skillBind[0]);
    c.Bind("skill_1_bind", &model.skillBind[1]);
    c.Bind("skill_2_bind", &model.skillBind[2]);
    c.Bind("skill_3_bind", &model.skillBind[3]);
    c.Bind("skill_4_bind", &model.skillBind[4]);
    c.Bind("hint_shown", &model.hintShown);
    c.Bind("bind_hint", &model.bindHint);
    c.Bind("helper_on", &model.helperOn);
    c.Bind("menu_open", &model.menuOpen);
    c.Bind("attack_label", &model.attackLabel);
    c.Bind("pickup_label", &model.pickupLabel);
    c.Bind("auto_label", &model.autoLabel);
    c.Bind("menu_label", &model.menuLabel);
    c.Bind("chat_label", &model.chatLabel);
    c.Bind("back_label", &model.backLabel);
    for (int item = 0; item < MenuItemCount; ++item)
        c.Bind(MenuLabelFields[item], &model.menuLabels[item]);
    UI::Mobile::HeroFrame::Bind(c, model.hero);
}

// Under every window and the HUD's bar, over the world's labels (RmlStackingOrder.cpp); shown
// without focus, so an open chat field keeps it.
UI::RmlBridge::ThemedView<ControlsModel> g_View{"touch_controls", BindModel,
                                                {{"Data/Interface/RmlUi/touch_controls.rml"}}};

struct ControlBox
{
    bool shown = false;
    float left = 0.f;
    float top = 0.f;
    float right = 0.f;
    float bottom = 0.f;
};

std::array<ControlBox, ControlCount> g_boxes{};
// Each control's press counter when last read: a press since shows as a difference.
std::array<std::uint32_t, ControlCount> g_seenPresses{};
bool g_shown = false;

// Putting a skill on a button, with no digit keys to bind it, either way round: a tap on an empty
// button (or on any while the skill list is open) opens the list and the skill picked there goes on
// that button; a skill picked first (the bar's current-skill icon opens the list) goes on the next
// button tapped.
struct SkillBinding
{
    // The skill list's pick count when last read (none yet on entering the world).
    std::optional<int> picksSeen;
    // The button the next skill picked goes on, or -1.
    int pendingSlot = -1;
    // Until when a skill picked with no button chosen waits for a tap on one.
    std::chrono::steady_clock::time_point until{};
    // The buttons whose finger has just put a skill there: they cast nothing until it lifts.
    std::array<bool, SkillButtons> noCast{};
};
SkillBinding g_binding;

float Dp()
{
    const Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    const float ratio = context != nullptr ? context->GetDensityIndependentPixelRatio() : 1.f;
    return ratio > 0.f ? ratio : 1.f;
}

bool Inside(const ControlBox& box, float x, float y, float slop)
{
    return box.shown && x >= box.left - slop && x <= box.right + slop && y >= box.top - slop &&
           y <= box.bottom + slop;
}

std::string_view FileName(const Rml::String& url)
{
    const std::string_view path(url);
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// Whether a document is drawn over the controls and keeps the fingers it covers (all but the
// pass-through ones).
bool IsDrawnOver(const Rml::ElementDocument* owner, const Rml::ElementDocument* controls)
{
    if (owner == nullptr || owner == controls)
        return false;
    const std::string_view name = FileName(owner->GetSourceURL());
    for (const std::string_view passThrough : PassThroughDocuments)
    {
        if (name == passThrough)
            return false;
    }
    const std::optional<float> depth = UI::RmlBridge::StackingDepthForDocument(name);
    const std::optional<float> ownDepth = UI::RmlBridge::StackingDepthForDocument(DocumentName);
    return !depth || !ownDepth || *depth > *ownDepth;
}

// Whether the element a finger landed on is a window over the controls (a document drawn above
// them, but for its empty body): that window takes the finger.
bool IsWindowOver(Rml::Element* hit, const Rml::ElementDocument* controls)
{
    if (hit == nullptr)
        return false;
    Rml::ElementDocument* owner = hit->GetOwnerDocument();
    return hit != owner && IsDrawnOver(owner, controls);
}

// Whether a window a workspace slot placed covers the point. Such a panel may take no pointer
// events (the inventory's item grid is clicked by its native code), so RmlUi's hit test looks
// through it to the controls below.
bool IsPlacedWindowAt(Rml::Context* context, const Rml::ElementDocument* controls, Rml::Vector2f point)
{
    for (int index = 0; index < context->GetNumDocuments(); ++index)
    {
        Rml::ElementDocument* other = context->GetDocument(index);
        if (other == nullptr || !other->IsVisible() || !IsDrawnOver(other, controls))
            continue;
        for (const char* rootId : {"panel", "content_root"})
        {
            Rml::Element* root = other->GetElementById(rootId);
            if (root != nullptr && root->IsClassSet("slot-placed") && UI::RmlBridge::IsPointWithin(root, point))
                return true;
        }
    }
    return false;
}

// Which control a finger landing at a window pixel belongs to.
int HitTest(float x, float y)
{
    Rml::ElementDocument* document = g_View.Document();
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (!g_shown || document == nullptr || context == nullptr)
        return Touch::NoControl;
    const Rml::Vector2f point(x, y);
    if (IsWindowOver(context->GetElementAtPoint(point), document) || IsPlacedWindowAt(context, document, point))
        return Touch::NoControl;

    const float slop = TouchSlopDp * Dp();
    // The buttons before the joystick's area, which they stand in; the menu's items, drawn over the
    // rest, first.
    for (int control = ControlCount - 1; control > StickControl; --control)
    {
        if (Inside(g_boxes[control], x, y, slop))
            return control;
    }
    return Inside(g_boxes[StickControl], x, y, 0.f) ? StickControl : Touch::NoControl;
}

void MeasureBoxes()
{
    Rml::ElementDocument* document = g_View.Document();
    for (int control = 0; control < ControlCount; ++control)
    {
        ControlBox& box = g_boxes[control];
        Rml::Element* element = document != nullptr ? document->GetElementById(ControlElement(control)) : nullptr;
        if (element == nullptr || !element->IsVisible(true))
        {
            box = ControlBox{};
            continue;
        }
        const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
        const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
        box = ControlBox{true, offset.x, offset.y, offset.x + size.x, offset.y + size.y};
    }
}

template <typename T> void SetField(T& field, const T& value, const char* name)
{
    if (field == value)
        return;
    field = value;
    g_View.MarkDirty(name);
}

void SyncLabel(Rml::String& field, const wchar_t* text, const char* name)
{
    SetField(field, StringUtils::WideToNarrow(text), name);
}

void SyncLabels(ControlsModel& model)
{
    SyncLabel(model.attackLabel, I18N::Game::TouchAttack, "attack_label");
    SyncLabel(model.pickupLabel, I18N::Game::TouchPickUp, "pickup_label");
    SyncLabel(model.autoLabel, I18N::Game::TouchAuto, "auto_label");
    SyncLabel(model.menuLabel, I18N::Game::TouchMenu, "menu_label");
    SyncLabel(model.chatLabel, I18N::Game::TouchChat, "chat_label");
    SyncLabel(model.backLabel, I18N::Game::Back, "back_label");
    for (int item = 0; item < MenuItemCount; ++item)
        SyncLabel(model.menuLabels[item], *MenuItems[item].label, MenuLabelFields[item].c_str());
}

void SyncSkills(ControlsModel& model)
{
    static constexpr const char* IconFields[SkillButtons] = {"skill_0_icon", "skill_1_icon", "skill_2_icon",
                                                             "skill_3_icon", "skill_4_icon"};
    static constexpr const char* CooldownFields[SkillButtons] = {
        "skill_0_cooldown", "skill_1_cooldown", "skill_2_cooldown", "skill_3_cooldown", "skill_4_cooldown"};
    static constexpr const char* SelectedFields[SkillButtons] = {
        "skill_0_selected", "skill_1_selected", "skill_2_selected", "skill_3_selected", "skill_4_selected"};
    static constexpr const char* EmptyFields[SkillButtons] = {"skill_0_empty", "skill_1_empty", "skill_2_empty",
                                                              "skill_3_empty", "skill_4_empty"};
    for (int slot = 0; slot < SkillButtons; ++slot)
    {
        const bool empty = g_pSkillList->GetHotKeySlotNumber(slot) < 0;
        SetField(model.skillEmpty[slot], empty, EmptyFields[slot]);
        SetField(model.skillIcons[slot], empty ? Rml::String("none") : g_pSkillList->GetHotKeySlotIconDecorator(slot),
                 IconFields[slot]);
        // Whole percents: the wipe redraws only when it moves.
        const float cooldown =
            empty ? 0.f : std::round(g_pSkillList->GetHotKeySlotCooldownFraction(slot) * 100.f) / 100.f;
        SetField(model.skillCooldowns[slot], cooldown, CooldownFields[slot]);
        SetField(model.skillSelected[slot], !empty && g_pSkillList->IsHotKeySlotCurrentSkill(slot),
                 SelectedFields[slot]);
    }
}

bool WaitingForButton()
{
    return std::chrono::steady_clock::now() < g_binding.until;
}

void UpdateBinding(ControlsModel& model)
{
    static constexpr const char* BindFields[SkillButtons] = {"skill_0_bind", "skill_1_bind", "skill_2_bind",
                                                             "skill_3_bind", "skill_4_bind"};
    const bool gridOpen = g_pSkillList->IsSkillGridOpen();
    const int picks = g_pSkillList->GetGridPickCount();
    const bool picked = g_binding.picksSeen && *g_binding.picksSeen != picks;
    g_binding.picksSeen = picks;
    if (picked && g_binding.pendingSlot >= 0)
    {
        // The pick made it the current skill (the list clicked already).
        GameLogic::Touch::BindHotKeySlot(g_binding.pendingSlot);
        g_binding.pendingSlot = -1;
    }
    else if (picked)
    {
        g_binding.until = std::chrono::steady_clock::now() + BindWindow;
    }
    else if (!gridOpen)
    {
        // Closed with nothing picked.
        g_binding.pendingSlot = -1;
    }

    const bool waiting = WaitingForButton();
    for (int slot = 0; slot < SkillButtons; ++slot)
        SetField(model.skillBind[slot], waiting || slot == g_binding.pendingSlot, BindFields[slot]);
    SetField(model.hintShown, gridOpen || waiting, "hint_shown");
    const wchar_t* hint = g_binding.pendingSlot >= 0 ? I18N::Game::TouchBindPickFor
                          : gridOpen                 ? I18N::Game::TouchBindPick
                                                     : I18N::Game::TouchBindPlace;
    SyncLabel(model.bindHint, hint, "bind_hint");
}

// Whether a finger landed on the control since the last frame; reading forgets it.
bool TakePress(int control)
{
    const std::uint32_t presses = Touch::Control(control).presses;
    const bool pressed = presses != g_seenPresses[control];
    g_seenPresses[control] = presses;
    return pressed;
}

void ForgetFingers()
{
    for (int control = 0; control < ControlCount; ++control)
        TakePress(control);
}

// The floating stick: its centre is where the finger landed; a finger beyond the knob's reach drags
// the centre along, so turning round needs no lift.
void UpdateStick(ControlsModel& model)
{
    const Touch::ControlFinger& finger = Touch::Control(StickControl);
    const bool landed = TakePress(StickControl);
    if (!finger.held)
    {
        if (model.stickActive)
        {
            GameLogic::Touch::StopSteering();
            SetField(model.knobX, 0.f, "knob_x");
            SetField(model.knobY, 0.f, "knob_y");
            SetField(model.stickActive, false, "stick_active");
        }
        return;
    }

    const float dp = Dp();
    const float reach = StickReachDp * dp;
    const bool fresh = landed || !model.stickActive;
    float centerX = fresh ? finger.downX : model.stickX;
    float centerY = fresh ? finger.downY : model.stickY;
    float dx = finger.x - centerX;
    float dy = finger.y - centerY;
    const float distance = std::hypot(dx, dy);
    if (distance > reach)
    {
        centerX += dx * (distance - reach) / distance;
        centerY += dy * (distance - reach) / distance;
        dx = finger.x - centerX;
        dy = finger.y - centerY;
    }
    SetField(model.stickActive, true, "stick_active");
    SetField(model.stickX, centerX, "stick_x");
    SetField(model.stickY, centerY, "stick_y");
    SetField(model.knobX, dx, "knob_x");
    SetField(model.knobY, dy, "knob_y");

    if (std::hypot(dx, dy) < StickDeadZoneDp * dp)
        GameLogic::Touch::StopSteering();
    else
        GameLogic::Touch::Steer(dx, dy);
}

void UpdateButtons(ControlsModel& model)
{
    TakePress(AttackControl);
    if (Touch::Control(AttackControl).held)
        GameLogic::Touch::Attack();

    for (int slot = 0; slot < SkillButtons; ++slot)
    {
        const int control = FirstSkillControl + slot;
        if (TakePress(control))
        {
            const bool gridOpen = g_pSkillList->IsSkillGridOpen();
            if (WaitingForButton() && GameLogic::Touch::BindHotKeySlot(slot))
            {
                PlayBuffer(SOUND_CLICK01);
                g_binding.until = {};
                g_binding.noCast[slot] = true;
            }
            else if (gridOpen || model.skillEmpty[slot])
            {
                // The skill picked in the list next goes here.
                g_binding.pendingSlot = slot;
                g_binding.until = {};
                g_binding.noCast[slot] = true;
                if (!gridOpen)
                    g_pSkillList->OnCurrentSkillClick();
            }
            else
            {
                GameLogic::Touch::SelectHotKeySlot(slot);
            }
        }
        if (!Touch::Control(control).held)
            g_binding.noCast[slot] = false;
        else if (!g_binding.noCast[slot])
            GameLogic::Touch::CastHotKeySlot(slot);
    }

    TakePress(PickupControl);
    if (Touch::Control(PickupControl).held)
        GameLogic::Touch::PickUpNearest();

    if (TakePress(AutoControl))
    {
        MUHelper::g_MuHelper.Toggle();
        PlayBuffer(SOUND_CLICK01);
    }

    for (int button = 0; button < KeyButtonCount; ++button)
    {
        if (TakePress(FirstKeyControl + button))
            Touch::TapKey(KeyButtons[button].key);
    }

    if (TakePress(MenuControl))
    {
        SetField(model.menuOpen, !model.menuOpen, "menu_open");
        PlayBuffer(SOUND_CLICK01);
    }
    for (int item = 0; item < MenuItemCount; ++item)
    {
        if (!TakePress(FirstMenuItemControl + item) || !model.menuOpen)
            continue;
        SetField(model.menuOpen, false, "menu_open");
        if (MenuItems[item].action != nullptr)
            MenuItems[item].action();
        else
            Touch::TapKey(MenuItems[item].key);
    }

    SetField(model.helperOn, MUHelper::g_MuHelper.IsActive(), "helper_on");
}

void Hide()
{
    if (!g_shown)
        return;
    g_shown = false;
    GameLogic::Touch::StopSteering();
    g_boxes.fill(ControlBox{});
    UI::RmlBridge::SyncDocumentVisibility(g_View.Document(), false);
}
} // namespace

namespace UI::HUD::TouchControls
{
void Update()
{
    if (!Touch::IsEnabled())
    {
        Hide();
        return;
    }

    const bool inWorld = SceneFlag == MAIN_SCENE && LoadingWorld < 30 && Hero != nullptr;
    if (!inWorld)
    {
        Hide();
        GameLogic::Touch::Reset();
        ForgetFingers();
        g_binding = SkillBinding{};
        return;
    }
    if (!g_View.Ensure())
        return;

    Touch::SetHitTest(&HitTest);
    Rml::ElementDocument* document = g_View.Document();
    if (document == nullptr)
        return;
    // Shown again after a theme switch rebuilt the document, too.
    if (!g_shown || !document->IsVisible())
    {
        g_shown = true;
        ForgetFingers();
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        // Two fingers zoom, and a phone has no F10 to lift the zoom lock (kept on a computer against
        // a stray wheel).
        if (CameraManager::Instance().IsZoomLocked())
            CameraManager::Instance().ToggleZoomLock();
    }

    ControlsModel& model = g_View.GetModel();
    SyncLabels(model);
    SyncSkills(model);
    UpdateBinding(model);
    UpdateStick(model);
    UpdateButtons(model);
    UI::Mobile::HeroFrame::Sync(model.hero, [](const char* field) { g_View.MarkDirty(field); });
    // The boxes the next fingers are tested against: where this frame lays the controls out.
    MeasureBoxes();
}
} // namespace UI::HUD::TouchControls
