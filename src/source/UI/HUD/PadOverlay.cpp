#include "stdafx.h"
#include "UI/HUD/PadOverlay.h"

#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"
#include "UI/HUD/PadGlyphs.h"
#include "UI/HUD/PadHub.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <array>
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

// The controls card's texts, card[0] .. in pad_overlay.rml, in this order.
const std::array<const wchar_t* const*, 29> CardLabels = {
    &I18N::Game::PadCardTitle,
    &I18N::Game::PadCardWorld,
    &I18N::Game::PadCardWindows,
    &I18N::Game::PadCardPage,
    &I18N::Game::PadCardWalk,
    &I18N::Game::PadCardTarget,
    &I18N::Game::PadCardPotions,
    &I18N::Game::PadCardFriend,
    &I18N::Game::PadCardHelper,
    &I18N::Game::PadCardPickUp,
    &I18N::Game::PadCardMap,
    &I18N::Game::PadCardBag,
    &I18N::Game::PadCardAttack,
    &I18N::Game::PadCardSkills,
    &I18N::Game::PadCardMoreSkills,
    &I18N::Game::PadCardSocial,
    &I18N::Game::PadCardMenu,
    &I18N::Game::PadCardPicker,
    &I18N::Game::PadCardFrame,
    &I18N::Game::PadCardPointer,
    &I18N::Game::PadCardNextWindow,
    &I18N::Game::PadCardNextPage,
    &I18N::Game::PadCardHubTabs,
    &I18N::Game::PadCardTake,
    &I18N::Game::PadCardUse,
    &I18N::Game::PadCardItemCard,
    &I18N::Game::PadCardClose,
    &I18N::Game::PadCardAllWindows,
    &I18N::Game::PadClose};

struct OverlayModel
{
    UI::HUD::PadGlyphs::Model glyphs;
    bool cardShown = false;
    int cardPage = 0;
    std::vector<Rml::String> card;
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
    bool legendSlider = false;
    bool legendItem = true;
    bool legendY = true;
    // Menu (every window): not over a question box.
    bool legendMenu = true;
    bool legendWindows = false;
    bool legendPages = false;
    Rml::String pickupLabel;
    // The prompt's button: A (an NPC or a player chosen), else d-pad down (a drop).
    bool promptA = false;
    bool charactersShown = false;
    Rml::String characterLabel;
    Rml::String playLabel;
    Rml::String createLabel;
    Rml::String deleteLabel;
    Rml::String menuLabel;
    Rml::String takeLabel;
    Rml::String sliderLabel;
    Rml::String equipLabel;
    Rml::String cardLabel;
    Rml::String closeLabel;
    Rml::String windowsLabel;
    Rml::String nextWindowLabel;
    Rml::String nextPageLabel;
};

void BindModel(Rml::DataModelConstructor& c, OverlayModel& model)
{
    auto tab = c.RegisterStruct<TabView>();
    tab.RegisterMember("label", &TabView::label);
    tab.RegisterMember("active", &TabView::active);
    c.RegisterArray<std::vector<TabView>>();

    c.RegisterArray<std::vector<Rml::String>>();

    UI::HUD::PadGlyphs::Bind(c, model.glyphs);
    c.Bind("card_shown", &model.cardShown);
    c.Bind("card_page", &model.cardPage);
    c.Bind("card", &model.card);
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
    c.Bind("legend_slider", &model.legendSlider);
    c.Bind("legend_item", &model.legendItem);
    c.Bind("legend_y", &model.legendY);
    c.Bind("legend_menu", &model.legendMenu);
    c.Bind("legend_windows", &model.legendWindows);
    c.Bind("legend_pages", &model.legendPages);
    c.Bind("next_window_label", &model.nextWindowLabel);
    c.Bind("next_page_label", &model.nextPageLabel);
    c.Bind("pickup_label", &model.pickupLabel);
    c.Bind("prompt_a", &model.promptA);
    c.Bind("characters_shown", &model.charactersShown);
    c.Bind("character_label", &model.characterLabel);
    c.Bind("play_label", &model.playLabel);
    c.Bind("create_label", &model.createLabel);
    c.Bind("delete_label", &model.deleteLabel);
    c.Bind("menu_label", &model.menuLabel);
    c.Bind("take_label", &model.takeLabel);
    c.Bind("slider_label", &model.sliderLabel);
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

void SyncCard(OverlayModel& model, const View& view)
{
    const bool opens = view.card && !model.cardShown;
    SyncBool(model.cardShown, view.card, "card_shown");
    if (model.cardPage != view.cardPage)
    {
        model.cardPage = view.cardPage;
        g_View.MarkDirty("card_page");
    }
    if (!opens && !model.card.empty())
        return;
    std::vector<Rml::String> card;
    card.reserve(CardLabels.size());
    for (const wchar_t* const* label : CardLabels)
        card.push_back(StringUtils::WideToNarrow(*label));
    if (card != model.card)
    {
        model.card = std::move(card);
        g_View.MarkDirty("card");
    }
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
    SyncBool(model.promptA, view.promptSays != Prompt::PickUp || view.promptByA, "prompt_a");
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

void SyncCharacters(OverlayModel& model, const View& view)
{
    SyncBool(model.charactersShown, view.characters, "characters_shown");
    if (!view.characters)
        return;
    SyncLabel(model.characterLabel, I18N::Game::PadCharacter, "character_label");
    SyncLabel(model.playLabel, I18N::Game::PadPlay, "play_label");
    SyncLabel(model.createLabel, I18N::Game::PadCreate, "create_label");
    SyncLabel(model.deleteLabel, I18N::Game::PadDelete, "delete_label");
    SyncLabel(model.menuLabel, I18N::Game::PadMenuPrompt, "menu_label");
}

void SyncLegend(OverlayModel& model, const View& view)
{
    SyncBool(model.legendShown, view.legend, "legend_shown");
    if (!view.legend)
        return;
    using Legend = View::Legend;
    const Legend says = view.legendSays;
    const bool asks = says == Legend::Answer || says == Legend::DialogButton;
    const bool onItem = says == Legend::Item || says == Legend::Worn || says == Legend::Sell || says == Legend::Move
                        || says == Legend::Crafting || says == Legend::Use || says == Legend::Keep;
    SyncBool(model.legendA, says != Legend::Empty && says != Legend::Slider, "legend_a");
    SyncBool(model.legendSlider, says == Legend::Slider, "legend_slider");
    SyncLabel(model.sliderLabel, I18N::Game::PadAdjust, "slider_label");
    SyncBool(model.legendItem, onItem && says != Legend::Keep, "legend_item");
    // The shop's goods have no card and nothing for X: A buys them. The crafting box's items have no card.
    SyncBool(model.legendY,
             says != Legend::Button && says != Legend::Goods && says != Legend::Empty && says != Legend::Crafting
                 && says != Legend::Slider && !asks,
             "legend_y");
    SyncBool(model.legendMenu, !asks, "legend_menu");
    SyncBool(model.legendWindows, view.legendWindows && !asks, "legend_windows");
    SyncBool(model.legendPages, view.legendPages && !asks, "legend_pages");
    SyncLabel(model.nextWindowLabel, I18N::Game::PadNextWindow, "next_window_label");
    SyncLabel(model.nextPageLabel, I18N::Game::PadNextPage, "next_page_label");
    const auto* aLabel = onItem                    ? &I18N::Game::PadTake
                         : says == Legend::Goods   ? &I18N::Game::PadBuy
                         : says == Legend::Holding ? &I18N::Game::PadPutDown
                         : says == Legend::Answer  ? &I18N::Game::PadDialogYes
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
    SyncLabel(model.closeLabel,
              says == Legend::Holding ? I18N::Game::PadPutBack : asks ? I18N::Game::PadDialogNo : I18N::Game::PadClose,
              "close_label");
    SyncLabel(model.windowsLabel, I18N::Game::PadAllWindows, "windows_label");
}
} // namespace

namespace UI::HUD::PadOverlay
{
void Update(const View& view)
{
    const bool shown = view.hub || view.card || view.legend || view.frame || view.prompt || view.characters;
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
    SyncCharacters(model, view);
    SyncCard(model, view);
    UI::HUD::PadGlyphs::Sync(model.glyphs, [](const char* name) { g_View.MarkDirty(name); });
    // Shown again after a theme switch rebuilt the document, too.
    if (!document->IsVisible())
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}
} // namespace UI::HUD::PadOverlay
