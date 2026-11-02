#include "stdafx.h"
#include "UI/HUD/PadOverlay.h"

#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"
#include "UI/HUD/PadHub.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cmath>
#include <string>
#include <vector>

namespace
{
using UI::HUD::PadOverlay::View;

// The frame stands this far outside its target's box, so it rings the cell rather than covering it.
constexpr float FrameOutset = 2.f;

struct TabView
{
    Rml::String label;
    bool active = false;
};

struct OverlayModel
{
    bool frameShown = false;
    Rml::String frameLeft;
    Rml::String frameTop;
    Rml::String frameWidth;
    Rml::String frameHeight;
    bool promptShown = false;
    Rml::String promptLeft;
    Rml::String promptTop;
    Rml::String topRight = "0px";
    bool hubOpen = false;
    std::vector<TabView> tabs;
    bool legendShown = false;
    // The legend on an item's place: X and Y too; with an item on the cursor Y (drop) too.
    bool legendA = true;
    bool legendItem = true;
    bool legendY = true;
    Rml::String pickupLabel;
    // The prompt's button: A (an NPC or a player chosen), else d-pad down (a drop).
    bool promptA = false;
    Rml::String takeLabel;
    Rml::String equipLabel;
    Rml::String cardLabel;
    Rml::String closeLabel;
    Rml::String windowsLabel;
};

void BindModel(Rml::DataModelConstructor& c, OverlayModel& model)
{
    auto tab = c.RegisterStruct<TabView>();
    tab.RegisterMember("label", &TabView::label);
    tab.RegisterMember("active", &TabView::active);
    c.RegisterArray<std::vector<TabView>>();

    c.Bind("frame_shown", &model.frameShown);
    c.Bind("frame_left", &model.frameLeft);
    c.Bind("frame_top", &model.frameTop);
    c.Bind("frame_width", &model.frameWidth);
    c.Bind("frame_height", &model.frameHeight);
    c.Bind("prompt_shown", &model.promptShown);
    c.Bind("prompt_left", &model.promptLeft);
    c.Bind("prompt_top", &model.promptTop);
    c.Bind("top_right", &model.topRight);
    c.Bind("hub_open", &model.hubOpen);
    c.Bind("tabs", &model.tabs);
    c.Bind("legend_shown", &model.legendShown);
    c.Bind("legend_a", &model.legendA);
    c.Bind("legend_item", &model.legendItem);
    c.Bind("legend_y", &model.legendY);
    c.Bind("pickup_label", &model.pickupLabel);
    c.Bind("prompt_a", &model.promptA);
    c.Bind("take_label", &model.takeLabel);
    c.Bind("equip_label", &model.equipLabel);
    c.Bind("card_label", &model.cardLabel);
    c.Bind("close_label", &model.closeLabel);
    c.Bind("windows_label", &model.windowsLabel);
}

// Over the windows and message boxes, under the item on the cursor (RmlStackingOrder.cpp); shown
// without focus, so the windows keep it.
UI::RmlBridge::ThemedView<OverlayModel> g_View{"pad_overlay", BindModel, {{"Data/Interface/RmlUi/pad_overlay.rml"}}};

void SyncBool(bool& field, bool value, const char* name)
{
    if (field == value)
        return;
    field = value;
    g_View.MarkDirty(name);
}

void SyncString(Rml::String& field, const Rml::String& value, const char* name)
{
    if (field == value)
        return;
    field = value;
    g_View.MarkDirty(name);
}

void SyncPixels(Rml::String& field, float value, const char* name)
{
    // Whole pixels: a box that does not move keeps its string, and nothing is marked.
    static Rml::String text;
    text = std::to_string(static_cast<int>(std::lround(value)));
    text += "px";
    SyncString(field, text, name);
}

void SyncLabel(Rml::String& field, const wchar_t* label, const char* name)
{
    SyncString(field, StringUtils::WideToNarrow(label), name);
}

void SyncFrame(OverlayModel& model, const View& view)
{
    SyncBool(model.frameShown, view.frame, "frame_shown");
    if (!view.frame)
        return;
    const UI::HUD::PadTargetPick::Box& box = view.frameBox;
    SyncPixels(model.frameLeft, box.left - FrameOutset, "frame_left");
    SyncPixels(model.frameTop, box.top - FrameOutset, "frame_top");
    SyncPixels(model.frameWidth, box.right - box.left + FrameOutset * 2.f, "frame_width");
    SyncPixels(model.frameHeight, box.bottom - box.top + FrameOutset * 2.f, "frame_height");
}

void SyncPrompt(OverlayModel& model, const View& view)
{
    SyncBool(model.promptShown, view.prompt, "prompt_shown");
    if (!view.prompt)
        return;
    SyncPixels(model.promptLeft, view.promptX, "prompt_left");
    SyncPixels(model.promptTop, view.promptY, "prompt_top");
    using Prompt = View::Prompt;
    SyncBool(model.promptA, view.promptSays != Prompt::PickUp, "prompt_a");
    SyncLabel(model.pickupLabel,
              view.promptSays == Prompt::Talk   ? I18N::Game::PadTalkPrompt
              : view.promptSays == Prompt::Menu ? I18N::Game::PadMenuPrompt
                                                : I18N::Game::PadPickUpPrompt,
              "pickup_label");
}

void SyncTabs(OverlayModel& model, const View& view)
{
    SyncPixels(model.topRight, view.topRight, "top_right");
    SyncBool(model.hubOpen, view.hub, "hub_open");
    if (!view.hub)
        return;
    const int count = UI::HUD::PadHub::TabCount();
    bool changed = static_cast<int>(model.tabs.size()) != count;
    model.tabs.resize(count);
    for (int i = 0; i < count; ++i)
    {
        TabView& tab = model.tabs[i];
        const Rml::String label = StringUtils::WideToNarrow(UI::HUD::PadHub::TabLabel(i));
        const bool active = i == UI::HUD::PadHub::CurrentTab();
        changed = changed || tab.label != label || tab.active != active;
        tab.label = label;
        tab.active = active;
    }
    if (changed)
        g_View.MarkDirty("tabs");
}

void SyncLegend(OverlayModel& model, const View& view)
{
    SyncBool(model.legendShown, view.legend, "legend_shown");
    if (!view.legend)
        return;
    using Legend = View::Legend;
    const Legend says = view.legendSays;
    const bool onItem = says == Legend::Item || says == Legend::Worn || says == Legend::Sell || says == Legend::Move
                        || says == Legend::Crafting || says == Legend::Use || says == Legend::Keep;
    SyncBool(model.legendA, says != Legend::Empty, "legend_a");
    SyncBool(model.legendItem, onItem && says != Legend::Keep, "legend_item");
    // The shop's goods have no card and nothing for X: A buys them. The crafting box's items have no card.
    SyncBool(model.legendY,
             says != Legend::Button && says != Legend::Goods && says != Legend::Empty && says != Legend::Crafting,
             "legend_y");
    const auto* aLabel = onItem                    ? &I18N::Game::PadTake
                         : says == Legend::Goods   ? &I18N::Game::PadBuy
                         : says == Legend::Holding ? &I18N::Game::PadPutDown
                                                   : &I18N::Game::PadPress;
    SyncLabel(model.takeLabel, *aLabel, "take_label");
    const auto* xLabel = says == Legend::Worn   ? &I18N::Game::TouchItemUnequip
                         : says == Legend::Sell ? &I18N::Game::TouchItemSell
                         : says == Legend::Use  ? &I18N::Game::Use
                         : says == Legend::Move || says == Legend::Crafting ? &I18N::Game::TouchItemMove
                                                : &I18N::Game::TouchItemEquip;
    SyncLabel(model.equipLabel, *xLabel, "equip_label");
    SyncLabel(model.cardLabel, view.legendSays == Legend::Holding ? I18N::Game::TouchItemDrop : I18N::Game::PadCard,
              "card_label");
    SyncLabel(model.closeLabel, view.legendSays == Legend::Holding ? I18N::Game::PadPutBack : I18N::Game::PadClose,
              "close_label");
    SyncLabel(model.windowsLabel, I18N::Game::PadAllWindows, "windows_label");
}
} // namespace

namespace UI::HUD::PadOverlay
{
void Update(const View& view)
{
    const bool shown = view.hub || view.legend || view.frame || view.prompt;
    if (!shown)
    {
        UI::RmlBridge::SyncDocumentVisibility(g_View.Document(), false);
        return;
    }
    if (!g_View.Ensure())
        return;
    Rml::ElementDocument* document = g_View.Document();
    if (document == nullptr)
        return;
    OverlayModel& model = g_View.GetModel();
    SyncFrame(model, view);
    SyncPrompt(model, view);
    SyncTabs(model, view);
    SyncLegend(model, view);
    // Shown again after a theme switch rebuilt the document, too.
    if (!document->IsVisible())
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}
} // namespace UI::HUD::PadOverlay
