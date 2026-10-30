#include "stdafx.h"
#include "UI/HUD/TouchControls.h"

#include "Audio/DSPlaySound.h"
#include "Camera/CameraManager.h"
#include "Core/Input/TouchInput.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzOpenData.h"
#include "GameLogic/Touch/TouchActions.h"
#include "I18N/All.h"
#include "MUHelper/MuHelper.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "UI/Dialogs/MessageBox.h"
#include "UI/HUD/SkillList.h"
#include "UI/HUD/Skills/SkillTooltip.h"
#include "UI/Mobile/HeroFrame.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/Mobile/PhoneHudLayout.h"
#include "UI/Mobile/SkillArrange.h"
#include "UI/Mobile/TouchUnit.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace Touch = Core::Input::Touch;
namespace SkillArrange = UI::Mobile::SkillArrange;

constexpr std::string_view DocumentName = "touch_controls.rml";
constexpr int SkillButtons = SkillArrange::SlotCount;
// The skill picker's entries a page holds at most: a tablet's. A phone's page holds fewer, as many as
// its grid fits.
constexpr int MaxPickEntries = 40;

// Back: the skill list's grid first, which Esc does not close (it would open the system menu over it),
// else Esc as a keyboard presses it.
void Back()
{
    if (g_pSkillList->IsSkillGridOpen())
    {
        g_pSkillList->OnCurrentSkillClick();
        return;
    }
    Touch::TapKey(VK_ESCAPE);
}

// Buttons that press a key, as a keyboard would: the potions drink the item hotkeys Q and W, the chat
// button opens the chat field (Enter), Back closes the window on top or, with none open, opens the
// system menu (Esc, as a phone has none), and the rest open their windows (UI/HUD/HotKey.cpp).
struct KeyButton
{
    const char* element;
    int key;
    // Done instead of the key, when set.
    void (*action)() = nullptr;
};

constexpr KeyButton KeyButtons[] = {
    {"tc_potion_q", 'Q'},  {"tc_potion_w", 'W'}, {"tc_chat", VK_RETURN}, {"tc_inventory", 'I'},
    {"tc_character", 'C'}, {"tc_move", 'M'},     {"tc_map", VK_TAB},     {"tc_back", VK_ESCAPE, &Back},
};
constexpr int KeyButtonCount = static_cast<int>(std::size(KeyButtons));

// The skill picker, on the first empty skill button (defined with the picker below).
void OpenPickerOnFreeButton();

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
    {&I18N::Game::TouchMenuSkills, 0, &OpenPickerOnFreeButton},
    {&I18N::Game::TouchMenuView, 0, &ToggleView},
    {&I18N::Game::TouchMenuSystem, VK_ESCAPE},
    // The chat line: with a pad, its keyboard (UI/HUD/PadKeyboard) comes up for it.
    {&I18N::Game::TouchChat, VK_RETURN},
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
    // The screen around the open menu: a tap there shuts it.
    MenuBackdropControl = FirstMenuItemControl + MenuItemCount,
    // The Skills button beside the skill buttons: it opens the skill picker.
    SkillEditControl,
    // The skill picker: the screen around it (a tap there shuts it), its panel (a tap between its
    // buttons does nothing), its buttons and the skills of the page it shows.
    PickerBackdropControl,
    PickerPanelControl,
    PickerClearControl,
    PickerDoneControl,
    PickerPrevControl,
    PickerNextControl,
    FirstPickControl,
    ControlCount = FirstPickControl + MaxPickEntries,
};
static_assert(ControlCount <= Touch::MaxControls);

constexpr const char* SkillElements[SkillButtons] = {"tc_skill_0", "tc_skill_1", "tc_skill_2", "tc_skill_3",
                                                     "tc_skill_4"};

// Numbered names, "<prefix>0" .. "<prefix><Count - 1>", made once.
template <int Count> std::array<std::string, Count> NumberedNames(std::string_view prefix)
{
    std::array<std::string, Count> names;
    for (int number = 0; number < Count; ++number)
        names[number] = std::string(prefix) + std::to_string(number);
    return names;
}

// The menu items' elements in the markup, and their label fields in the model.
const std::array<std::string, MenuItemCount> MenuItemElements = NumberedNames<MenuItemCount>("tc_menu_");
const std::array<std::string, MenuItemCount> MenuLabelFields = NumberedNames<MenuItemCount>("menu_label_");
// The picker's entries, in the order of the page's skills (their ids are bound, PickerEntry::id).
const std::array<std::string, MaxPickEntries> PickElements = NumberedNames<MaxPickEntries>("tc_pick_");

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
    case MenuBackdropControl:
        return "tc_menu_backdrop";
    case SkillEditControl:
        return "tc_skill_edit";
    case PickerBackdropControl:
        return "tc_picker_backdrop";
    case PickerPanelControl:
        return "tc_picker";
    case PickerClearControl:
        return "tc_pick_clear";
    case PickerDoneControl:
        return "tc_pick_done";
    case PickerPrevControl:
        return "tc_pick_prev";
    case PickerNextControl:
        return "tc_pick_next";
    default:
        break;
    }
    if (control >= FirstSkillControl && control < FirstSkillControl + SkillButtons)
        return SkillElements[control - FirstSkillControl];
    if (control >= FirstKeyControl && control < FirstMenuItemControl)
        return KeyButtons[control - FirstKeyControl].element;
    if (control >= FirstPickControl)
        return PickElements[control - FirstPickControl].c_str();
    return MenuItemElements[control - FirstMenuItemControl].c_str();
}

// How far outside its box a finger still presses a button, in finger units (UI::Mobile::TouchUnit,
// the unit touch_controls.rcss sizes the controls in): a thumb is wider than its aim.
constexpr float TouchSlopUnits = 6.f;
// The knob goes no further from the stick's centre than this, in finger units; a finger further out
// drags the centre along.
constexpr float StickReachUnits = 48.f;
// A finger closer than this to the stick's centre, in finger units, does not walk.
constexpr float StickDeadZoneUnits = 10.f;

// Documents over the controls that still leave them the finger, as they only show text: the chat's
// log lies over the joystick's corner, the scrolling notice's band across the top row.
constexpr std::string_view PassThroughDocuments[] = {"chat_log.rml", "slide_notice.rml"};

// How long a skill picked in the skill list waits for a tap on a skill button to go on.
constexpr auto BindWindow = std::chrono::seconds(8);

// The gap between the skill picker's entries, in finger units: touch_controls.rcss's #tc_pick_grid.
constexpr float PickGapUnits = 6.f;
// How far outside its box a finger still presses a button of the picker's panel, in finger units:
// less than elsewhere, the panel's buttons lie close to its entries.
constexpr float PickSlopUnits = 3.f;

// A skill the picker offers.
struct PickerEntry
{
    // The entry's element: tc_pick_<n>, n its place on the page.
    Rml::String id;
    Rml::String icon;
    Rml::String name;
    // On one of the five skill buttons; on the one being set.
    bool onButton = false;
    bool here = false;

    bool operator==(const PickerEntry&) const = default;
};

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
    // A gamepad in use: its buttons' glyphs show on the controls, and the menu's highlighted item
    // (-1 none) with its legend.
    bool pad = false;
    // Fingers drive the controls: the joystick's area shows.
    bool fingers = false;
    int menuFocus = -1;
    Rml::String padOpenLabel;
    Rml::String padCloseLabel;
    // The skill picker: open, the skill button being set (lit on the arc), the page of skills shown,
    // and the pad's highlight (its element's id, empty for none).
    bool pickerOpen = false;
    int pickerSlot = 0;
    std::vector<PickerEntry> pickerEntries;
    bool pickerNoSkills = false;
    bool pickerCanClear = false;
    bool pickerPages = false;
    Rml::String pickerPage;
    Rml::String pickerHint;
    Rml::String pickerTip;
    Rml::String pickerFocus;
    Rml::String pickerTitle;
    Rml::String pickerClearLabel;
    Rml::String pickerDoneLabel;
    Rml::String pickerNoSkillsLabel;
    Rml::String padChooseLabel;
    Rml::String skillsLabel;
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
    c.Bind("pad", &model.pad);
    c.Bind("fingers", &model.fingers);
    c.Bind("menu_focus", &model.menuFocus);
    c.Bind("pad_open_label", &model.padOpenLabel);
    c.Bind("pad_close_label", &model.padCloseLabel);

    auto entry = c.RegisterStruct<PickerEntry>();
    entry.RegisterMember("id", &PickerEntry::id);
    entry.RegisterMember("icon", &PickerEntry::icon);
    entry.RegisterMember("name", &PickerEntry::name);
    entry.RegisterMember("on_button", &PickerEntry::onButton);
    entry.RegisterMember("here", &PickerEntry::here);
    c.RegisterArray<std::vector<PickerEntry>>();
    c.Bind("picker_open", &model.pickerOpen);
    c.Bind("picker_slot", &model.pickerSlot);
    c.Bind("picker_entries", &model.pickerEntries);
    c.Bind("picker_no_skills", &model.pickerNoSkills);
    c.Bind("picker_can_clear", &model.pickerCanClear);
    c.Bind("picker_pages", &model.pickerPages);
    c.Bind("picker_page", &model.pickerPage);
    c.Bind("picker_hint", &model.pickerHint);
    c.Bind("picker_tip", &model.pickerTip);
    c.Bind("picker_focus", &model.pickerFocus);
    c.Bind("picker_title", &model.pickerTitle);
    c.Bind("picker_clear_label", &model.pickerClearLabel);
    c.Bind("picker_done_label", &model.pickerDoneLabel);
    c.Bind("picker_no_skills_label", &model.pickerNoSkillsLabel);
    c.Bind("pad_choose_label", &model.padChooseLabel);
    c.Bind("skills_label", &model.skillsLabel);
    UI::Mobile::HeroFrame::Bind(c, model.hero);
}

// Under every window and the HUD's bar, over the world's labels (RmlStackingOrder.cpp); shown
// without focus, so an open chat field keeps it.
UI::RmlBridge::ThemedView<ControlsModel> g_View{
    "touch_controls", BindModel, {{"Data/Interface/RmlUi/touch_controls.rml"}}};

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

// What a gamepad holds on the controls (UI/HUD/GamepadControls), beside the fingers: a control is
// held while a finger or the pad holds it, and pressed by either.
struct PadPresses
{
    std::array<bool, ControlCount> held{};
    std::array<std::uint32_t, ControlCount> presses{};
    std::array<std::uint32_t, ControlCount> seen{};
};
PadPresses g_pad;

// Putting a skill on a button from the game's own skill list, as the PC layout's bar opens it (the
// skill picker below is the touch controls' own way): while the list is open, a tap on a button and
// then a skill puts the skill there; a skill picked first goes on the next button tapped.
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

// The skill picker (Menu -> Skills, the Skills button, a tap on an empty skill button): a panel of the
// character's skills left of the skill buttons, which stay lit over the dimmed screen. A tap on a button
// chooses it, a tap on a skill puts the skill there (one already on another button swaps with it) and
// chooses the next empty button.
struct SkillPicker
{
    // The skills of the page shown, by entry: what a tap on an entry puts on the button.
    std::array<int, MaxPickEntries> shown{};
    int shownCount = 0;
    int page = 0;
    // How many entries a page holds, as the grid was last laid out.
    int perPage = MaxPickEntries;
    // The page turns to the chosen button's skill (on opening, and as a button is chosen), until the
    // player turns it.
    bool pageFollowsSlot = true;
    // The pad's highlight: a control, or none.
    int focus = Touch::NoControl;
    // The skills' names, looked up once a showing.
    std::map<int, std::wstring> names;
};
SkillPicker g_picker;

bool Inside(const ControlBox& box, float x, float y, float slop)
{
    return box.shown && x >= box.left - slop && x <= box.right + slop && y >= box.top - slop && y <= box.bottom + slop;
}

std::string_view FileName(const Rml::String& url)
{
    const std::string_view path(url);
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// The depth a document is drawn at now: its z-index, which differs from the table's for a few (the
// phone view's HUD overlays, these controls while their menu is open).
std::optional<float> CurrentDepth(Rml::ElementDocument* document)
{
    const Rml::Property* depth = document->GetLocalProperty(Rml::PropertyId::ZIndex);
    if (depth != nullptr && depth->unit == Rml::Unit::NUMBER)
        return depth->Get<float>();
    return UI::RmlBridge::StackingDepthForDocument(FileName(document->GetSourceURL()));
}

// Whether a document is drawn over the controls and keeps the fingers it covers (all but the
// pass-through ones).
bool IsDrawnOver(Rml::ElementDocument* owner, Rml::ElementDocument* controls)
{
    if (owner == nullptr || owner == controls)
        return false;
    const std::string_view name = FileName(owner->GetSourceURL());
    for (const std::string_view passThrough : PassThroughDocuments)
    {
        if (name == passThrough)
            return false;
    }
    const std::optional<float> depth = CurrentDepth(owner);
    const std::optional<float> ownDepth = CurrentDepth(controls);
    return !depth || !ownDepth || *depth > *ownDepth;
}

// Whether the element a finger landed on is a window over the controls (a document drawn above
// them, but for its empty body): that window takes the finger.
bool IsWindowOver(Rml::Element* hit, Rml::ElementDocument* controls)
{
    if (hit == nullptr)
        return false;
    Rml::ElementDocument* owner = hit->GetOwnerDocument();
    return hit != owner && IsDrawnOver(owner, controls);
}

// Whether a window a workspace slot placed covers the point. Such a panel may take no pointer
// events (the inventory's item grid is clicked by its native code), so RmlUi's hit test looks
// through it to the controls below.
bool IsPlacedWindowAt(Rml::Context* context, Rml::ElementDocument* controls, Rml::Vector2f point)
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

// While the menu is open only it takes fingers: its items, the menu button that shuts it, and the
// screen around it, which shuts it too (a tap meant to close it walks nowhere).
int MenuHitTest(float x, float y, float slop)
{
    for (int control = FirstMenuItemControl; control < FirstMenuItemControl + MenuItemCount; ++control)
    {
        if (Inside(g_boxes[control], x, y, slop))
            return control;
    }
    if (Inside(g_boxes[MenuControl], x, y, slop))
        return MenuControl;
    return Inside(g_boxes[MenuBackdropControl], x, y, 0.f) ? MenuBackdropControl : Touch::NoControl;
}

// While the skill picker is open only it takes fingers: its skills and buttons, its panel around them,
// the skill buttons it sets, and the screen around, which shuts it.
int PickerHitTest(float x, float y, float slop)
{
    for (int control = FirstPickControl; control < ControlCount; ++control)
    {
        if (Inside(g_boxes[control], x, y, 0.f))
            return control;
    }
    const float panelSlop = PickSlopUnits * UI::Mobile::TouchUnit::Pixels();
    for (const int control : {PickerDoneControl, PickerClearControl, PickerPrevControl, PickerNextControl})
    {
        if (Inside(g_boxes[control], x, y, panelSlop))
            return control;
    }
    if (Inside(g_boxes[PickerPanelControl], x, y, 0.f))
        return PickerPanelControl;
    for (int control = FirstSkillControl; control < FirstSkillControl + SkillButtons; ++control)
    {
        if (Inside(g_boxes[control], x, y, slop))
            return control;
    }
    return Inside(g_boxes[PickerBackdropControl], x, y, 0.f) ? PickerBackdropControl : Touch::NoControl;
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

    const float slop = TouchSlopUnits * UI::Mobile::TouchUnit::Pixels();
    if (g_View.GetModel().pickerOpen)
        return PickerHitTest(x, y, slop);
    if (g_View.GetModel().menuOpen)
        return MenuHitTest(x, y, slop);
    // The buttons before the joystick's area, which they stand in; the Skills button after the skill
    // buttons it lies beside, so a thumb between them casts.
    for (int control = FirstMenuItemControl - 1; control > StickControl; --control)
    {
        if (Inside(g_boxes[control], x, y, slop))
            return control;
    }
    if (Inside(g_boxes[SkillEditControl], x, y, slop))
        return SkillEditControl;
    return Inside(g_boxes[StickControl], x, y, 0.f) ? StickControl : Touch::NoControl;
}

ControlBox BoxOf(Rml::ElementDocument* document, const char* id)
{
    Rml::Element* element = document != nullptr ? document->GetElementById(id) : nullptr;
    if (element == nullptr || !element->IsVisible(true))
        return ControlBox{};
    const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
    return ControlBox{true, offset.x, offset.y, offset.x + size.x, offset.y + size.y};
}

void MeasureBoxes()
{
    Rml::ElementDocument* document = g_View.Document();
    for (int control = 0; control < ControlCount; ++control)
        g_boxes[control] = BoxOf(document, ControlElement(control));

    // A picker entry the grid cuts off (a page laid out before the grid was measured) takes no finger.
    const ControlBox grid = BoxOf(document, "tc_pick_grid");
    for (int control = FirstPickControl; control < ControlCount; ++control)
    {
        ControlBox& box = g_boxes[control];
        if (box.shown && (!grid.shown || box.right > grid.right + 1.f || box.bottom > grid.bottom + 1.f))
            box.shown = false;
    }
}

// The phone's HUD pieces in other documents (the buffs, the pets' frames, the system messages, the
// skill list) go beside the controls as this frame lays them out.
void FollowNeighbours(Rml::ElementDocument* document)
{
    if (!UI::Mobile::ReplacesPcHud())
    {
        UI::Mobile::PhoneHudLayout::Release();
        return;
    }
    const ControlBox topRow = BoxOf(document, "tc_quick");
    const ControlBox hero = BoxOf(document, "tc_hero");
    if (!topRow.shown || !hero.shown)
        return;
    const float screenWidth = document->GetBox().GetSize(Rml::BoxArea::Border).x;
    const float screenHeight = document->GetBox().GetSize(Rml::BoxArea::Border).y;

    UI::Mobile::PhoneHudLayout::Edges edges;
    edges.topRowBottom = topRow.bottom;
    edges.heroLeft = hero.left;
    edges.heroRight = hero.right;
    edges.heroBottom = hero.bottom;
    edges.potionsTop = screenHeight;
    edges.potionsRight = hero.left;
    for (const char* potion : {"tc_potion_q", "tc_potion_w"})
    {
        const ControlBox box = BoxOf(document, potion);
        if (!box.shown)
            continue;
        edges.potionsTop = std::min(edges.potionsTop, box.top);
        edges.potionsRight = std::max(edges.potionsRight, box.right);
    }
    // The skill buttons and the Skills button beside them.
    edges.skillsLeft = screenWidth;
    const auto keepLeftOf = [&edges](const ControlBox& box)
    {
        if (box.shown)
            edges.skillsLeft = std::min(edges.skillsLeft, box.left);
    };
    for (int slot = 0; slot < SkillButtons; ++slot)
        keepLeftOf(g_boxes[FirstSkillControl + slot]);
    keepLeftOf(g_boxes[SkillEditControl]);
    UI::Mobile::PhoneHudLayout::Follow(edges);
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
    SyncLabel(model.padOpenLabel, I18N::Game::PadOpen, "pad_open_label");
    SyncLabel(model.padCloseLabel, I18N::Game::PadClose, "pad_close_label");
    SyncLabel(model.padChooseLabel, I18N::Game::PadChoose, "pad_choose_label");
    SyncLabel(model.skillsLabel, I18N::Game::TouchMenuSkills, "skills_label");
    SyncLabel(model.pickerTitle, I18N::Game::TouchPickerTitle, "picker_title");
    SyncLabel(model.pickerClearLabel, I18N::Game::TouchPickerClear, "picker_clear_label");
    SyncLabel(model.pickerDoneLabel, I18N::Game::TouchPickerDone, "picker_done_label");
    SyncLabel(model.pickerNoSkillsLabel, I18N::Game::TouchPickerNoSkills, "picker_no_skills_label");
    for (int item = 0; item < MenuItemCount; ++item)
        SyncLabel(model.menuLabels[item], *MenuItems[item].label, MenuLabelFields[item].c_str());
}

// The devices the controls show for: the joystick's area for fingers, the glyphs for a pad.
void SyncDevices(ControlsModel& model)
{
    SetField(model.pad, UI::Mobile::IsGamepadInUse(), "pad");
    SetField(model.fingers, Touch::IsEnabled(), "fingers");
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

// The line over the skill buttons while one is being set, in the words of the device in use.
const wchar_t* BindHint(bool gridOpen)
{
    if (g_binding.pendingSlot >= 0)
        return I18N::Game::TouchBindPickFor;
    const bool pad = UI::Mobile::IsGamepadInUse();
    if (gridOpen)
        return pad ? I18N::Game::PadBindPick : I18N::Game::TouchBindPick;
    return pad ? I18N::Game::PadBindPlace : I18N::Game::TouchBindPlace;
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
    SyncLabel(model.bindHint, BindHint(gridOpen), "bind_hint");
}

// A dialog that waits for an answer (the system menu, a confirmation, a message box): the menu
// shuts and stays shut while one is up, where its items would take the taps meant for the dialog.
bool IsDialogOpen()
{
    return !g_MessageBox->IsEmpty() || mu::ui::window::g_pGenericConfirmDialog->IsVisible() ||
           mu::ui::window::g_pGenericMenuDialog->IsVisible();
}

// Whether a finger landed on the control or the pad pressed it since the last frame; reading forgets it.
bool TakePress(int control)
{
    const std::uint32_t presses = Touch::Control(control).presses;
    const bool pressed = presses != g_seenPresses[control] || g_pad.presses[control] != g_pad.seen[control];
    g_seenPresses[control] = presses;
    g_pad.seen[control] = g_pad.presses[control];
    return pressed;
}

// Whether a finger or the pad holds the control.
bool IsHeld(int control)
{
    return Touch::Control(control).held || g_pad.held[control];
}

void HoldForPad(int control, bool held)
{
    if (control < 0 || control >= ControlCount || g_pad.held[control] == held)
        return;
    g_pad.held[control] = held;
    if (held)
        ++g_pad.presses[control];
}

// The control of a button that presses a key (KeyButtons), by its element.
int KeyControl(std::string_view element)
{
    for (int button = 0; button < KeyButtonCount; ++button)
    {
        if (element == KeyButtons[button].element)
            return FirstKeyControl + button;
    }
    return Touch::NoControl;
}

int PadControl(UI::HUD::TouchControls::PadButton button)
{
    using UI::HUD::TouchControls::PadButton;
    switch (button)
    {
    case PadButton::Attack:
        return AttackControl;
    case PadButton::PickUp:
        return PickupControl;
    case PadButton::Auto:
        return AutoControl;
    case PadButton::Menu:
        return MenuControl;
    case PadButton::PotionQ:
        return KeyControl("tc_potion_q");
    case PadButton::PotionW:
        return KeyControl("tc_potion_w");
    case PadButton::Map:
        return KeyControl("tc_map");
    case PadButton::Back:
        return KeyControl("tc_back");
    default:
        break;
    }
    const int skill = static_cast<int>(button) - static_cast<int>(PadButton::Skill0);
    return skill >= 0 && skill < SkillButtons ? FirstSkillControl + skill : Touch::NoControl;
}

struct BoxCentre
{
    float x = 0.f;
    float y = 0.f;
};

BoxCentre CentreOf(const ControlBox& box)
{
    return {(box.left + box.right) / 2.f, (box.top + box.bottom) / 2.f};
}

// The control among `candidates` next to `from` the way (dx, dy) points: the nearest along that way,
// a step to either side counting double. One in line with `from` comes first (beside it in its row for
// a step left or right, in its column for a step up or down): a step right off a row of the skill
// picker lands on the skill button beside that row, not on a nearer button above it. `from` itself
// when none lies that way.
int NextThatWay(std::span<const int> candidates, int from, int dx, int dy)
{
    const ControlBox& origin = g_boxes[from];
    if (!origin.shown)
        return from;
    const BoxCentre start = CentreOf(origin);
    const auto inLine = [&origin, dx](const ControlBox& box)
    {
        return dx != 0 ? box.top < origin.bottom && box.bottom > origin.top
                       : box.left < origin.right && box.right > origin.left;
    };
    for (const bool lineOnly : {true, false})
    {
        int best = from;
        float bestScore = 0.f;
        for (const int control : candidates)
        {
            const ControlBox& box = g_boxes[control];
            if (control == from || !box.shown || (lineOnly && !inLine(box)))
                continue;
            const BoxCentre centre = CentreOf(box);
            const float along =
                (centre.x - start.x) * static_cast<float>(dx) + (centre.y - start.y) * static_cast<float>(dy);
            if (along < 1.f)
                continue;
            const float across = dx != 0 ? std::abs(centre.y - start.y) : std::abs(centre.x - start.x);
            const float score = along + 2.f * across;
            if (best == from || score < bestScore)
            {
                best = control;
                bestScore = score;
            }
        }
        if (best != from)
            return best;
    }
    return from;
}

// The menu item next to `from` the way (dx, dy) points, or `from`.
int NextMenuItem(int from, int dx, int dy)
{
    static const std::array<int, MenuItemCount> items = []
    {
        std::array<int, MenuItemCount> controls{};
        for (int item = 0; item < MenuItemCount; ++item)
            controls[item] = FirstMenuItemControl + item;
        return controls;
    }();
    return NextThatWay(items, FirstMenuItemControl + from, dx, dy) - FirstMenuItemControl;
}

// Drops the finger presses made while the controls were away. The pad's stay: when a pad press is
// what brought the controls up, it acts.
void ForgetFingers()
{
    for (int control = 0; control < ControlCount; ++control)
        g_seenPresses[control] = Touch::Control(control).presses;
}

// Lets go of the pad's holds and drops its presses: off the world nothing waits for them.
void ForgetPad()
{
    g_pad.held.fill(false);
    g_pad.seen = g_pad.presses;
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

    const float unit = UI::Mobile::TouchUnit::Pixels();
    const float reach = StickReachUnits * unit;
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

    if (std::hypot(dx, dy) < StickDeadZoneUnits * unit)
        GameLogic::Touch::StopSteering();
    else
        GameLogic::Touch::Steer(dx, dy);
}

// A button that opens or shuts a window takes the pointer the last tap left off the window: a window
// opening there would show the tooltip of whatever lies under it. The pad's pointer is the player's own
// and stays.
void RestFingerPointer()
{
    if (!UI::Mobile::IsGamepadInUse())
        Touch::RestPointer();
}

// The hotkeys as the skill list holds them.
SkillArrange::HotKeys ReadHotKeys()
{
    SkillArrange::HotKeys keys{};
    for (int hotKey = 0; hotKey < SkillArrange::HotKeyCount; ++hotKey)
        keys[hotKey] = g_pSkillList->GetHotKey(hotKey);
    return keys;
}

// Writes the hotkeys that changed and saves them at once: a closed browser tab is a logout the client
// never sees. CSkillList::SetHotKey takes the skill off any other hotkey first, which never undoes one
// written before: each skill is on one hotkey at most.
void WriteHotKeys(const SkillArrange::HotKeys& before, const SkillArrange::HotKeys& after)
{
    for (int hotKey = 0; hotKey < SkillArrange::HotKeyCount; ++hotKey)
    {
        if (before[hotKey] != after[hotKey])
            g_pSkillList->SetHotKey(hotKey, after[hotKey]);
    }
    SaveOptions();
}

// The first empty skill button, else the first: where the picker starts.
int SlotToSet()
{
    const SkillArrange::HotKeys keys = ReadHotKeys();
    const bool upper = g_pSkillList->IsSkillListUp();
    if (keys[SkillArrange::HotKeyOfSlot(0, upper)] == SkillArrange::NoSkill)
        return 0;
    return SkillArrange::NextEmptySlot(keys, upper, 0);
}

// A skill's or pet command's name, as its tooltip heads it; looked up once a showing.
const std::wstring& SkillName(int skill)
{
    const auto known = g_picker.names.find(skill);
    if (known != g_picker.names.end())
        return known->second;
    std::wstring name;
    // Kept off the stack: a tooltip's lines are too big for the browser build's.
    static UI::Skills::Tooltip::Model tooltip;
    if (UI::Skills::Tooltip::BuildModelForSlot(skill, tooltip))
    {
        for (int line = 0; line < tooltip.count; ++line)
        {
            if (!tooltip.lines[line].isBlank)
            {
                name = tooltip.lines[line].text;
                break;
            }
        }
    }
    return g_picker.names.emplace(skill, std::move(name)).first->second;
}

void OpenPicker(ControlsModel& model, int slot)
{
    // The game's own list would wait under the picker for a pick.
    if (g_pSkillList->IsSkillGridOpen())
        g_pSkillList->OnCurrentSkillClick();
    g_binding.pendingSlot = -1;
    g_binding.until = {};
    g_picker.names.clear();
    g_picker.pageFollowsSlot = true;
    g_picker.focus = Touch::NoControl;
    SetField(model.menuOpen, false, "menu_open");
    SetField(model.menuFocus, -1, "menu_focus");
    SetField(model.pickerSlot, slot, "picker_slot");
    SetField(model.pickerOpen, true, "picker_open");
    PlayBuffer(SOUND_CLICK01);
    RestFingerPointer();
}

void OpenPickerOnFreeButton()
{
    OpenPicker(g_View.GetModel(), SlotToSet());
}

void ClosePicker(ControlsModel& model)
{
    if (!model.pickerOpen)
        return;
    SetField(model.pickerOpen, false, "picker_open");
    SetField(model.pickerFocus, Rml::String(), "picker_focus");
    g_picker.focus = Touch::NoControl;
    RestFingerPointer();
}

// Where the pad's highlight starts: on the chosen button's skill, else on the first skill.
int PickerHomeFocus(const ControlsModel& model)
{
    const int shown = std::min(g_picker.shownCount, static_cast<int>(model.pickerEntries.size()));
    for (int entry = 0; entry < shown; ++entry)
    {
        if (model.pickerEntries[entry].here)
            return FirstPickControl + entry;
    }
    return shown > 0 ? FirstPickControl : PickerDoneControl;
}

// What the pad's highlight moves over: the page's skills, the panel's buttons and the skill buttons on
// the right, which it chooses.
std::vector<int> PickerFocusables()
{
    std::vector<int> controls;
    for (int entry = 0; entry < g_picker.shownCount; ++entry)
        controls.push_back(FirstPickControl + entry);
    for (const int control : {PickerPrevControl, PickerNextControl, PickerClearControl, PickerDoneControl})
        controls.push_back(control);
    for (int slot = 0; slot < SkillButtons; ++slot)
        controls.push_back(FirstSkillControl + slot);
    return controls;
}

void MovePickerFocus(ControlsModel& model, int dx, int dy)
{
    if (g_picker.focus == Touch::NoControl)
    {
        // Lit as the picker opens, without a click of its own.
        g_picker.focus = PickerHomeFocus(model);
    }
    else if (dx != 0 || dy != 0)
    {
        const int next = NextThatWay(PickerFocusables(), g_picker.focus, dx, dy);
        if (next == g_picker.focus)
            return;
        g_picker.focus = next;
        PlayBuffer(SOUND_CLICK01);
    }
    SetField(model.pickerFocus, Rml::String(ControlElement(g_picker.focus)), "picker_focus");
}

// The page of skills the picker shows, the line over it, and the pad's highlight kept on what shows.
void ShowPickerPage(ControlsModel& model, const SkillArrange::HotKeys& keys, bool upper)
{
    const std::vector<int> listed = g_pSkillList->GetListedSkills();
    const int listedCount = static_cast<int>(listed.size());
    const int perPage = std::clamp(g_picker.perPage, 1, MaxPickEntries);
    const int pages = SkillArrange::PageCount(listedCount, perPage);
    const int hotKey = SkillArrange::HotKeyOfSlot(model.pickerSlot, upper);
    const int held = keys[hotKey];
    if (g_picker.pageFollowsSlot && held != SkillArrange::NoSkill)
    {
        const auto found = std::find(listed.begin(), listed.end(), held);
        if (found != listed.end())
            g_picker.page = static_cast<int>(found - listed.begin()) / perPage;
    }
    // Turned past either end, the pages go round.
    g_picker.page = ((g_picker.page % pages) + pages) % pages;

    const int first = g_picker.page * perPage;
    g_picker.shownCount = std::clamp(listedCount - first, 0, perPage);
    std::vector<PickerEntry> entries;
    entries.reserve(static_cast<size_t>(g_picker.shownCount));
    for (int entry = 0; entry < g_picker.shownCount; ++entry)
    {
        const int skill = listed[static_cast<size_t>(first + entry)];
        g_picker.shown[entry] = skill;
        const int on = SkillArrange::HotKeyOf(keys, skill);
        bool onButton = false;
        for (int slot = 0; slot < SkillButtons; ++slot)
            onButton = onButton || on == SkillArrange::HotKeyOfSlot(slot, upper);
        entries.push_back({PickElements[entry], g_pSkillList->GetSkillIconDecorator(skill),
                           StringUtils::WideToNarrow(SkillName(skill).c_str()), onButton, on == hotKey});
    }
    if (entries != model.pickerEntries)
    {
        model.pickerEntries = std::move(entries);
        g_View.MarkDirty("picker_entries");
    }

    SetField(model.pickerNoSkills, listed.empty(), "picker_no_skills");
    SetField(model.pickerCanClear, held != SkillArrange::NoSkill, "picker_can_clear");
    SetField(model.pickerPages, pages > 1, "picker_pages");
    SetField(model.pickerPage, Rml::String(std::to_string(g_picker.page + 1) + " / " + std::to_string(pages)),
             "picker_page");
    wchar_t hint[256];
    if (held != SkillArrange::NoSkill)
        mu_swprintf_s(hint, I18N::Game::TouchPickerOnButton, SkillName(held).c_str());
    else
        mu_swprintf_s(hint, L"%ls", I18N::Game::TouchPickerPickFor);
    SyncLabel(model.pickerHint, hint, "picker_hint");
    SyncLabel(model.pickerTip, UI::Mobile::IsGamepadInUse() ? I18N::Game::PadPickerTip : I18N::Game::TouchPickerTip,
              "picker_tip");

    // A highlight on what no longer shows moves to what does.
    int& focus = g_picker.focus;
    if (focus >= FirstPickControl && focus - FirstPickControl >= g_picker.shownCount)
        focus = g_picker.shownCount > 0 ? FirstPickControl + g_picker.shownCount - 1 : PickerDoneControl;
    if ((focus == PickerClearControl && !model.pickerCanClear) ||
        ((focus == PickerPrevControl || focus == PickerNextControl) && !model.pickerPages))
        focus = PickerDoneControl;
    SetField(model.pickerFocus, Rml::String(focus == Touch::NoControl ? "" : ControlElement(focus)), "picker_focus");
}

// The picker's taps and the pad's presses on it, then the page it shows.
void UpdatePicker(ControlsModel& model)
{
    const bool backdrop = TakePress(PickerBackdropControl);
    const bool done = TakePress(PickerDoneControl);
    // The pad's Menu button shuts it as it shuts the menu.
    const bool menu = TakePress(MenuControl);
    TakePress(PickerPanelControl);
    TakePress(SkillEditControl);
    if (done || menu)
        PlayBuffer(SOUND_CLICK01);
    if (backdrop || done || menu || IsDialogOpen())
    {
        ClosePicker(model);
        return;
    }

    const bool upper = g_pSkillList->IsSkillListUp();
    SkillArrange::HotKeys keys = ReadHotKeys();
    const auto put = [&keys](int hotKey, int skill)
    {
        const SkillArrange::HotKeys after = SkillArrange::Placed(keys, hotKey, skill);
        if (after == keys)
            return false;
        WriteHotKeys(keys, after);
        keys = after;
        return true;
    };

    for (int slot = 0; slot < SkillButtons; ++slot)
    {
        if (!TakePress(FirstSkillControl + slot))
            continue;
        // Chosen, not cast: the finger on it casts nothing until it lifts, the picker shut or not.
        g_binding.noCast[slot] = true;
        if (slot != model.pickerSlot)
            PlayBuffer(SOUND_CLICK01);
        SetField(model.pickerSlot, slot, "picker_slot");
        g_picker.pageFollowsSlot = true;
    }
    if (TakePress(PickerClearControl) &&
        put(SkillArrange::HotKeyOfSlot(model.pickerSlot, upper), SkillArrange::NoSkill))
        PlayBuffer(SOUND_CLICK01);

    const bool previous = TakePress(PickerPrevControl);
    const bool next = TakePress(PickerNextControl);
    if (previous != next)
    {
        g_picker.page += next ? 1 : -1;
        g_picker.pageFollowsSlot = false;
        PlayBuffer(SOUND_CLICK01);
    }

    for (int entry = 0; entry < MaxPickEntries; ++entry)
    {
        if (!TakePress(FirstPickControl + entry) || entry >= g_picker.shownCount)
            continue;
        PlayBuffer(SOUND_CLICK01);
        if (!put(SkillArrange::HotKeyOfSlot(model.pickerSlot, upper), g_picker.shown[entry]))
            continue;
        // On to the next empty button: filling them all is a tap a skill.
        g_picker.pageFollowsSlot = false;
        SetField(model.pickerSlot, SkillArrange::NextEmptySlot(keys, upper, model.pickerSlot), "picker_slot");
    }

    ShowPickerPage(model, keys, upper);
}

// How many skills a page holds: as many entries as the grid fits, measured on its first.
void MeasurePickerPage(Rml::ElementDocument* document)
{
    const ControlBox& entry = g_boxes[FirstPickControl];
    Rml::Element* grid = document->GetElementById("tc_pick_grid");
    if (!entry.shown || grid == nullptr)
        return;
    const Rml::Vector2f size = grid->GetBox().GetSize(Rml::BoxArea::Content);
    const float gap = PickGapUnits * UI::Mobile::TouchUnit::Pixels();
    g_picker.perPage =
        std::clamp(SkillArrange::PageCapacity(size.x, size.y, entry.right - entry.left, entry.bottom - entry.top, gap),
                   1, MaxPickEntries);
}

// The menu button opens and shuts the menu, a tap beside it shuts it, an item opens its window and
// shuts it.
void UpdateMenu(ControlsModel& model)
{
    const bool dialogOpen = IsDialogOpen();
    if (TakePress(MenuControl) && !dialogOpen)
    {
        SetField(model.menuOpen, !model.menuOpen, "menu_open");
        PlayBuffer(SOUND_CLICK01);
    }
    if (TakePress(MenuBackdropControl) || dialogOpen)
        SetField(model.menuOpen, false, "menu_open");
    if (!model.menuOpen)
        SetField(model.menuFocus, -1, "menu_focus");
    for (int item = 0; item < MenuItemCount; ++item)
    {
        if (!TakePress(FirstMenuItemControl + item) || !model.menuOpen)
            continue;
        SetField(model.menuOpen, false, "menu_open");
        RestFingerPointer();
        if (MenuItems[item].action != nullptr)
            MenuItems[item].action();
        else
            Touch::TapKey(MenuItems[item].key);
    }
}

void UpdateButtons(ControlsModel& model)
{
    TakePress(AttackControl);
    if (IsHeld(AttackControl))
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
            else if (gridOpen)
            {
                // The skill picked in the list next goes here.
                g_binding.pendingSlot = slot;
                g_binding.until = {};
                g_binding.noCast[slot] = true;
            }
            else if (model.skillEmpty[slot])
            {
                // Nothing to cast: the picker opens to fill it.
                g_binding.noCast[slot] = true;
                OpenPicker(model, slot);
            }
            else
            {
                GameLogic::Touch::SelectHotKeySlot(slot);
            }
        }
        if (!IsHeld(control))
            g_binding.noCast[slot] = false;
        else if (!g_binding.noCast[slot])
            GameLogic::Touch::CastHotKeySlot(slot);
    }
    if (TakePress(SkillEditControl))
        OpenPicker(model, SlotToSet());

    TakePress(PickupControl);
    if (IsHeld(PickupControl))
        GameLogic::Touch::PickUpNearest();

    if (TakePress(AutoControl))
    {
        MUHelper::g_MuHelper.Toggle();
        PlayBuffer(SOUND_CLICK01);
    }

    for (int button = 0; button < KeyButtonCount; ++button)
    {
        if (!TakePress(FirstKeyControl + button))
            continue;
        RestFingerPointer();
        if (KeyButtons[button].action != nullptr)
            KeyButtons[button].action();
        else
            Touch::TapKey(KeyButtons[button].key);
    }

    UpdateMenu(model);

    SetField(model.helperOn, MUHelper::g_MuHelper.IsActive(), "helper_on");
}

// Over every window while the menu or the skill picker is open (UI::RmlBridge::OpenTouchMenuDepth), so
// its panel is seen whole and takes the fingers; under them otherwise.
void SyncDepth(Rml::ElementDocument* document, bool panelOpen)
{
    const float depth = panelOpen ? UI::RmlBridge::OpenTouchMenuDepth
                                 : UI::RmlBridge::StackingDepthForDocument(DocumentName).value_or(0.f);
    const Rml::Property* current = document->GetLocalProperty(Rml::PropertyId::ZIndex);
    if (current == nullptr || current->Get<float>() != depth)
        document->SetProperty(Rml::PropertyId::ZIndex, Rml::Property(depth, Rml::Unit::NUMBER));
}

void Hide()
{
    if (!g_shown)
        return;
    g_shown = false;
    GameLogic::Touch::StopSteering();
    ClosePicker(g_View.GetModel());
    g_boxes.fill(ControlBox{});
    UI::Mobile::PhoneHudLayout::Release();
    UI::RmlBridge::SyncDocumentVisibility(g_View.Document(), false);
}
} // namespace

namespace UI::HUD::TouchControls
{
void Update()
{
    if (!UI::Mobile::ShowsControls())
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
        ForgetPad();
        g_binding = SkillBinding{};
        return;
    }
    if (!g_View.Ensure())
        return;

    Touch::SetHitTest(&HitTest);
    Rml::ElementDocument* document = g_View.Document();
    if (document == nullptr)
        return;
    // Sized for fingers on this screen before it is first laid out, and as the screen changes.
    UI::Mobile::TouchUnit::Apply(document);
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
    SyncDevices(model);
    SyncSkills(model);
    UpdateBinding(model);
    UpdateStick(model);
    if (model.pickerOpen)
        UpdatePicker(model);
    else
        UpdateButtons(model);
    SyncDepth(document, model.menuOpen || model.pickerOpen);
    UI::Mobile::HeroFrame::Sync(model.hero, [](const char* field) { g_View.MarkDirty(field); });
    // The boxes the next fingers are tested against: where this frame lays the controls out.
    MeasureBoxes();
    if (model.pickerOpen)
        MeasurePickerPage(document);
    FollowNeighbours(document);
}

void SetPadHeld(PadButton button, bool held)
{
    HoldForPad(PadControl(button), held);
}

int ControlAt(float x, float y)
{
    const int control = HitTest(x, y);
    return control == StickControl ? Touch::NoControl : control;
}

void SetControlHeld(int control, bool held)
{
    HoldForPad(control, held);
}

void ReleasePad()
{
    g_pad.held.fill(false);
}

bool IsMenuOpen()
{
    return g_shown && (g_View.GetModel().menuOpen || g_View.GetModel().pickerOpen);
}

void CloseMenu()
{
    ControlsModel& model = g_View.GetModel();
    SetField(model.menuOpen, false, "menu_open");
    SetField(model.menuFocus, -1, "menu_focus");
    ClosePicker(model);
}

void MoveMenuFocus(int dx, int dy)
{
    if (!IsMenuOpen())
        return;
    ControlsModel& model = g_View.GetModel();
    if (model.pickerOpen)
    {
        MovePickerFocus(model, dx, dy);
        return;
    }
    // The first item lights as the menu opens, without a click of its own: the menu's was heard.
    const bool first = model.menuFocus < 0;
    const int focus = first ? 0 : NextMenuItem(model.menuFocus, dx, dy);
    if (focus == model.menuFocus)
        return;
    if (!first)
        PlayBuffer(SOUND_CLICK01);
    SetField(model.menuFocus, focus, "menu_focus");
}

void ActivateMenuFocus()
{
    if (!IsMenuOpen())
        return;
    if (g_View.GetModel().pickerOpen)
    {
        if (g_picker.focus >= 0 && g_picker.focus < ControlCount)
            ++g_pad.presses[g_picker.focus];
        return;
    }
    const int focus = g_View.GetModel().menuFocus;
    if (focus >= 0 && focus < MenuItemCount)
        ++g_pad.presses[FirstMenuItemControl + focus];
}

bool OpenSkillPicker()
{
    if (!g_shown)
        return false;
    OpenPickerOnFreeButton();
    return true;
}

void PressBack()
{
    if (!g_shown)
        Touch::TapKey(VK_ESCAPE);
    else if (IsMenuOpen())
        CloseMenu();
    else
        Back();
}
} // namespace UI::HUD::TouchControls
