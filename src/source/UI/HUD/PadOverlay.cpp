#include "stdafx.h"
#include "UI/HUD/PadOverlay.h"

#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"
#include "Scenes/SceneCore.h" // SceneFlag
#include "UI/HUD/PadGlyphs.h"
#include "UI/HUD/PadHub.h"
#include "UI/HUD/TouchControls.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Property.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace
{
using UI::HUD::PadOverlay::View;

// The frame stands this far outside its target's box, so it rings the cell rather than covering it.
constexpr float FrameOutset = 2.f;
// The frame glides to its next target: each frame it goes this share of the way left per this many
// seconds (1 - e^(-dt / GlideTau)), so it is 95 % there after about 70 ms at any frame rate.
constexpr float GlideTau = 0.023f;
// Closer than this (pixels) it stands on the target.
constexpr float GlideRest = 0.5f;
// A move further than this share of the screen jumps (the next window far off): a long glide looks slow.
constexpr float GlideJumpShare = 0.5f;
// How often the labels are converted again even when they kept their text's address: a language
// switched may write new texts in place (as TouchControls' labels, 4 a second).
constexpr auto LabelRefresh = std::chrono::milliseconds(250);

struct TabView
{
    Rml::String label;
    bool active = false;
};

// The controls card's texts, card[0] .. in pad_overlay.rml, in this order.
const std::array<const wchar_t* const*, 31> CardLabels = {
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
    &I18N::Game::PadClose,
    &I18N::Game::PadCardLayoutLine,
    &I18N::Game::PadCardSkillsThird};

struct OverlayModel
{
    UI::HUD::PadGlyphs::Model glyphs;
    bool cardShown = false;
    int cardPage = 0;
    std::vector<Rml::String> card;
    // RT + B casts the sixth skill (a pad alone on the lower set): the card shows B in that row and says 4 to 6.
    bool cardSix = true;
    // RB + X, Y, B cast skills 7 to 9 (the same pad alone on the lower set, pad round 6): the card's row for them.
    bool cardNine = true;
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
    bool offerShown = false;
    Rml::String offerLabel;
    Rml::String offerHoldLabel;
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
    c.Bind("card_six", &model.cardSix);
    c.Bind("card_nine", &model.cardNine);
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
    c.Bind("offer_shown", &model.offerShown);
    c.Bind("offer_label", &model.offerLabel);
    c.Bind("offer_hold_label", &model.offerHoldLabel);
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

// The text each label field was last converted from, so an unchanged one is not converted every frame;
// forgotten every LabelRefresh.
struct LabelSeen
{
    const Rml::String* field = nullptr;
    const wchar_t* label = nullptr;
};
std::vector<LabelSeen> g_labelsSeen;
std::chrono::steady_clock::time_point g_labelsSeenAt{};

void ForgetLabelsNowAndThen()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - g_labelsSeenAt < LabelRefresh)
        return;
    g_labelsSeenAt = now;
    g_labelsSeen.clear();
}

// Whether the field was last converted from this very text; remembers it otherwise.
bool SeenLabel(const Rml::String& field, const wchar_t* label)
{
    const auto seen = std::find_if(g_labelsSeen.begin(), g_labelsSeen.end(),
                                   [&field](const LabelSeen& entry) { return entry.field == &field; });
    if (seen == g_labelsSeen.end())
    {
        g_labelsSeen.push_back({&field, label});
        return false;
    }
    if (seen->label == label)
        return true;
    seen->label = label;
    return false;
}

void SyncLabel(Rml::String& field, const wchar_t* label, const char* name)
{
    if (SeenLabel(field, label))
        return;
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
    const bool six = UI::HUD::TouchControls::SkillButtonsInPlay() > 5;
    SyncBool(model.cardSix, six, "card_six");
    SyncBool(model.cardNine, UI::HUD::TouchControls::SkillButtonsInPlay() > 6, "card_nine");
    std::vector<Rml::String> card;
    card.reserve(CardLabels.size());
    for (const wchar_t* const* label : CardLabels)
    {
        const bool moreSkills = label == &I18N::Game::PadCardMoreSkills;
        card.push_back(StringUtils::WideToNarrow(moreSkills && !six ? I18N::Game::PadCardMoreSkillsFive : *label));
    }
    if (card != model.card)
    {
        model.card = std::move(card);
        g_View.MarkDirty("card");
    }
}

using UI::HUD::PadTargetPick::Box;

// Where the frame is drawn now, on its way to its target's box, and whether it was drawn last frame.
Box g_frameDrawn;
bool g_frameWasDrawn = false;

bool FarApart(const Box& a, const Box& b, const Rml::Vector2i& screen)
{
    const float dx = std::fabs(a.CentreX() - b.CentreX());
    const float dy = std::fabs(a.CentreY() - b.CentreY());
    return dx > static_cast<float>(screen.x) * GlideJumpShare || dy > static_cast<float>(screen.y) * GlideJumpShare;
}

float Approach(float from, float to, float share)
{
    const float at = from + (to - from) * share;
    return std::fabs(to - at) < GlideRest ? to : at;
}

// The frame's box this frame: on its target at once when it was hidden or the target is far off, else
// gliding there.
Box GlideFrame(const Box& target, bool wasShown, float seconds, const Rml::Vector2i& screen)
{
    if (!wasShown || FarApart(g_frameDrawn, target, screen))
        return target;
    const float share = 1.f - std::exp(-seconds / GlideTau);
    return {Approach(g_frameDrawn.left, target.left, share), Approach(g_frameDrawn.top, target.top, share),
            Approach(g_frameDrawn.right, target.right, share), Approach(g_frameDrawn.bottom, target.bottom, share)};
}

void SyncFrame(OverlayModel& model, const View& view, float seconds, const Rml::Vector2i& screen)
{
    const bool wasShown = g_frameWasDrawn;
    g_frameWasDrawn = view.frame;
    SyncBool(model.frameShown, view.frame, "frame_shown");
    if (!view.frame)
        return;
    g_frameDrawn = GlideFrame(view.frameBox, wasShown, seconds, screen);
    const Box& box = g_frameDrawn;
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
    // Tabs added or taken away: their fields are new, none of them converted yet.
    if (changed)
        g_labelsSeen.clear();
    model.tabs.resize(count);
    for (int i = 0; i < count; ++i)
    {
        TabView& tab = model.tabs[i];
        const bool active = i == UI::HUD::PadHub::CurrentTab();
        changed = changed || tab.active != active;
        tab.active = active;
        const wchar_t* text = UI::HUD::PadHub::TabLabel(i);
        if (SeenLabel(tab.label, text))
            continue;
        const Rml::String label = StringUtils::WideToNarrow(text);
        changed = changed || tab.label != label;
        tab.label = label;
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
    // No item's card and nothing for X there: a control, the pointer on no target, an item to repair.
    const bool noCard = says == Legend::Button || says == Legend::Pointer || says == Legend::Repair;
    SyncBool(model.legendA, says != Legend::Empty && says != Legend::Slider, "legend_a");
    SyncBool(model.legendSlider, says == Legend::Slider, "legend_slider");
    SyncLabel(model.sliderLabel, I18N::Game::PadAdjust, "slider_label");
    SyncBool(model.legendItem, onItem && says != Legend::Keep, "legend_item");
    // The shop's goods have no card and nothing for X: A buys them. The crafting box's items have no card.
    SyncBool(model.legendY,
             !noCard && says != Legend::Goods && says != Legend::Empty && says != Legend::Crafting
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
                         : says == Legend::Repair  ? &I18N::Game::Repair
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

// The overlay is over the reconnect dialog (SyncDepth()).
bool g_overReconnect = false;

// The overlay's depth: over the reconnect dialog while it is up (the frame on its Cancel; pad round 8, A4),
// then back at the scene's own (RmlStackingOrder.h), which RmlTheme gives again on a scene change only.
void SyncDepth(Rml::ElementDocument* document, bool overReconnect)
{
    if (document == nullptr || (!overReconnect && !g_overReconnect))
        return;
    g_overReconnect = overReconnect;
    constexpr std::string_view Name = "pad_overlay.rml";
    std::optional<float> depth;
    if (overReconnect)
        depth = UI::RmlBridge::PadOverlayOverReconnectDepth();
    else if (SceneFlag == MAIN_SCENE)
        depth = UI::RmlBridge::StackingDepthForDocument(Name);
    else
        depth = UI::RmlBridge::StackingDepthOutsideMainScene(Name);
    if (!depth)
        return;
    const Rml::Property* current = document->GetLocalProperty(Rml::PropertyId::ZIndex);
    if (current == nullptr || current->Get<float>() != *depth)
        document->SetProperty(Rml::PropertyId::ZIndex, Rml::Property(*depth, Rml::Unit::NUMBER));
}

void SyncOffer(OverlayModel& model, const View& view)
{
    const bool shown = view.offerText != nullptr;
    SyncBool(model.offerShown, shown, "offer_shown");
    if (!shown)
        return;
    // The offer's line is built anew for each item in the same buffer: compared as text, not by its address.
    SyncString(model.offerLabel, StringUtils::WideToNarrow(view.offerText), "offer_label");
    SyncLabel(model.offerHoldLabel, I18N::Game::PadOfferEquipHold, "offer_hold_label");
}
} // namespace

namespace UI::HUD::PadOverlay
{
void Update(const View& view, float seconds)
{
    const bool shown = view.hub || view.card || view.legend || view.frame || view.prompt || view.characters
                       || view.offerText != nullptr;
    if (!shown)
    {
        g_frameWasDrawn = false;
        SyncDepth(g_View.Document(), view.overReconnect);
        UI::RmlBridge::SyncDocumentVisibility(g_View.Document(), false);
        return;
    }
    if (!g_View.Ensure())
        return;
    Rml::ElementDocument* document = g_View.Document();
    if (document == nullptr)
        return;
    SyncDepth(document, view.overReconnect);
    OverlayModel& model = g_View.GetModel();
    ForgetLabelsNowAndThen();
    const Rml::Context* context = document->GetContext();
    SyncFrame(model, view, seconds, context != nullptr ? context->GetDimensions() : Rml::Vector2i{});
    SyncPrompt(model, view);
    SyncTabs(model, view);
    SyncLegend(model, view);
    SyncCharacters(model, view);
    SyncOffer(model, view);
    SyncCard(model, view);
    UI::HUD::PadGlyphs::Sync(model.glyphs, [](const char* name) { g_View.MarkDirty(name); });
    // Shown again after a theme switch rebuilt the document, too.
    if (!document->IsVisible())
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}
} // namespace UI::HUD::PadOverlay
