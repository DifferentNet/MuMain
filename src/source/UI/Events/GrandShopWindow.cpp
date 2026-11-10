#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/GrandShopWindow.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Events/GrandShop.h"
#include "GameLogic/Items/CSItemOption.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/ConfirmRequest.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cstdarg>
#include <cwchar>
#include <utility>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::GrandShop::Grand;
using GameLogic::GrandShop::Kind;
using GameLogic::GrandShop::Line;

namespace
{
// Without an answer this long after asking, the server does not send the Grand Reset shop.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// The line above the note takes this many lines at most: the server's refusals are longer than one.
constexpr size_t MaxHintLines = 3;
// The standing note takes this many lines at most.
constexpr size_t MaxNoteLines = 3;
constexpr wchar_t JewelKey[] = L"jewel";
constexpr int NoSelection = -1;

enum eHINT_KIND
{
    HINT_INFO = 0,
    HINT_GOOD = 1,
    HINT_BAD = 2,
};

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

// 5000000 -> "5,000,000".
std::wstring Thousands(long long value)
{
    std::wstring digits = std::to_wstring(value < 0 ? -value : value);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
        digits.insert(static_cast<size_t>(i), L",");
    return value < 0 ? L"-" + digits : digits;
}

std::wstring Printf(const wchar_t* format, ...)
{
    wchar_t text[512] = {};
    va_list args;
    va_start(args, format);
    std::vswprintf(text, std::size(text), format, args);
    va_end(args);
    return text;
}

// A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
GrandShopLine MakeLine(const std::wstring& text, int width, bool bold = false)
{
    if (text.empty())
        return {};
    g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
    const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
    return {StringUtils::WideToNarrow(text.c_str()),
            UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                             static_cast<float>(width))};
}

// The text word by word on lines of the width, maxLines at most (the last one takes the rest).
std::vector<std::wstring> WrapWords(const std::wstring& text, int width, size_t maxLines)
{
    g_pRenderText->SetFont(g_hFont);
    std::vector<std::wstring> lines;
    std::wstring current;
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find(L' ', start);
        if (end == std::wstring::npos)
            end = text.size();
        const std::wstring word = text.substr(start, end - start);
        start = end + 1;
        if (word.empty())
            continue;
        const std::wstring candidate = current.empty() ? word : current + L" " + word;
        if (!current.empty() && MeasureInReferenceUnits(candidate) > width && lines.size() + 1 < maxLines)
        {
            lines.push_back(current);
            current = word;
        }
        else
        {
            current = candidate;
        }
    }
    if (!current.empty())
        lines.push_back(current);
    return lines;
}

std::vector<GrandShopLine> MakeWrapped(const std::wstring& text, size_t maxLines)
{
    std::vector<GrandShopLine> lines;
    for (const auto& part : WrapWords(text, CGrandShopWindow::CONTENT_WIDTH, maxLines))
        lines.push_back(MakeLine(part, CGrandShopWindow::CONTENT_WIDTH));
    return lines;
}

int PieceItemType(const Line& line)
{
    return line.Group * MAX_ITEM_INDEX + line.Number;
}

// The piece's name: the client's own ancient name for its type and set type, else the set's name from the server in
// front of the item's name; the jewel is just its item's name.
std::wstring ItemName(const Line& line)
{
    const int type = PieceItemType(line);
    if (type < 0 || type >= MAX_ITEM)
        return line.SetName;

    if (line.LineKind != Kind::Jewel)
    {
        wchar_t ancientName[64] = {};
        if (g_csItemOption.GetAncientItemName(ancientName, std::size(ancientName), type, line.SetType))
            return ancientName;
    }

    const std::wstring itemName = ItemAttribute[type].Name;
    return line.LineKind == Kind::Jewel || line.SetName.empty() ? itemName : line.SetName + L" " + itemName;
}

std::wstring SetKeyOf(const Line& line)
{
    if (line.LineKind == Kind::Jewel)
        return JewelKey;
    return std::to_wstring(static_cast<int>(line.LineKind)) + L":" + line.SetName;
}

// The reason the line cannot be bought now; empty when it can.
std::wstring WhyNotBuyable(const Line& line)
{
    const auto& status = Grand().GetStatus();
    if (!status.SafeZone)
        return I18N::Game::GrandShopNotSafeZone;
    if (status.Marks < line.Price)
        return I18N::Game::GrandShopNoMarks;
    return {};
}

void BindLineStructs(Rml::DataModelConstructor& c)
{
    auto lineType = c.RegisterStruct<GrandShopLine>();
    lineType.RegisterMember("text", &GrandShopLine::text);
    lineType.RegisterMember("text_px", &GrandShopLine::textPx);
    c.RegisterArray<std::vector<GrandShopLine>>();
}

void BindListStructs(Rml::DataModelConstructor& c)
{
    auto setType = c.RegisterStruct<GrandShopSet>();
    setType.RegisterMember("name", &GrandShopSet::name);
    setType.RegisterMember("top", &GrandShopSet::top);
    setType.RegisterMember("index", &GrandShopSet::index);
    setType.RegisterMember("selected", &GrandShopSet::selected);
    c.RegisterArray<std::vector<GrandShopSet>>();

    auto itemType = c.RegisterStruct<GrandShopItem>();
    itemType.RegisterMember("id", &GrandShopItem::id);
    itemType.RegisterMember("name", &GrandShopItem::name);
    itemType.RegisterMember("price", &GrandShopItem::price);
    itemType.RegisterMember("index", &GrandShopItem::index);
    itemType.RegisterMember("can_buy", &GrandShopItem::canBuy);
    itemType.RegisterMember("hint", &GrandShopItem::hint);
    c.RegisterArray<std::vector<GrandShopItem>>();
}
} // namespace

mu::ui::window::CGrandShopWindow::CGrandShopWindow() = default;

mu::ui::window::CGrandShopWindow::~CGrandShopWindow()
{
    Release();
}

bool mu::ui::window::CGrandShopWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_GRAND_SHOP, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CGrandShopWindow::Release()
{
    m_ItemTarget.Disable();
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CGrandShopWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CGrandShopWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CGrandShopWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_seenAnswers = Grand().Answers();
    m_showAnswer = false;
    m_pickedSet.clear();
}

void mu::ui::window::CGrandShopWindow::ClosingProcess()
{
    m_ItemTarget.Disable();
}

bool mu::ui::window::CGrandShopWindow::UpdateMouseEvent()
{
    // The sets, the buttons and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CGrandShopWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_GRAND_SHOP))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_GRAND_SHOP);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CGrandShopWindow::Update()
{
    const bool exit = std::exchange(m_pendingExit, false);
    const int set = std::exchange(m_pendingSet, NoSelection);
    const int buy = std::exchange(m_pendingBuy, NoSelection);
    if (IsVisible())
    {
        TakeClicks(exit, set, buy);
        TakeAnswers();
    }

    SyncRmlModel();
    return true;
}

void mu::ui::window::CGrandShopWindow::TakeClicks(bool exit, int set, int buy)
{
    if (exit)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_GRAND_SHOP);
        PlayBuffer(SOUND_CLICK01);
        return;
    }

    if (set >= 0 && set < static_cast<int>(m_setKeys.size()))
    {
        m_pickedSet = m_setKeys[static_cast<size_t>(set)];
        m_showAnswer = false;
        PlayBuffer(SOUND_CLICK01);
    }

    if (buy < 0 || buy >= static_cast<int>(m_itemKeys.size()) || !Grand().HasInfo() || Grand().IsBusy())
        return;

    // The button is off when the purchase cannot go through, but the click is checked here all the same.
    for (const auto& line : Grand().Lines())
    {
        if (line.Key == m_itemKeys[static_cast<size_t>(buy)] && WhyNotBuyable(line).empty())
        {
            AskToBuy(line);
            return;
        }
    }
}

void mu::ui::window::CGrandShopWindow::AskToBuy(const Line& line)
{
    PlayBuffer(SOUND_CLICK01);

    UI::Dialogs::ConfirmRequest request;
    request.lines.push_back({Printf(I18N::Game::GrandShopConfirm, ItemName(line).c_str(), Thousands(line.Price).c_str()), false});
    request.acceptLabel = I18N::Game::GrandShopBuy;
    request.cancellable = true;
    request.onAccept = [key = line.Key]
    {
        Grand().Buy(key);
    };
    m_showAnswer = false;
    UI::Dialogs::ShowConfirm(std::move(request));
}

void mu::ui::window::CGrandShopWindow::TakeAnswers()
{
    if (Grand().Answers() == m_seenAnswers)
        return;

    m_seenAnswers = Grand().Answers();
    m_showAnswer = true;
}

bool mu::ui::window::CGrandShopWindow::Render()
{
    // Nothing native: the frame, the lines and the buttons are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CGrandShopWindow::BindRmlModel(Rml::DataModelConstructor& c, GrandShopRmlModel& model)
{
    BindLineStructs(c);
    BindListStructs(c);
    c.Bind("title", &model.title);
    c.Bind("marks", &model.marks);
    c.Bind("has_info", &model.hasInfo);
    c.Bind("sets", &model.sets);
    c.Bind("items", &model.items);
    c.Bind("top_label", &model.topLabel);
    c.Bind("buy_label", &model.buyLabel);
    c.Bind("hint", &model.hint);
    c.Bind("hint_kind", &model.hintKind);
    c.Bind("note", &model.note);
    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("grand_shop_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("grand_shop_set",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingSet = args[0].Get<int>(NoSelection);
                        });
    c.BindEventCallback("grand_shop_buy",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingBuy = args[0].Get<int>(NoSelection);
                        });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CGrandShopWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    Rml::ElementDocument* document = m_RmlView.Document();
    if (!document)
        return;

    // Layer depth UI::RmlBridge::ForegroundPanelLayerDepth: over the HUD and its logs.
    UI::RmlBridge::SyncDocumentVisibilityInFront(document, IsVisible());
    const bool shown = IsVisible();
    m_ItemTarget.Sync(document->GetElementById("gs_items"), shown && Grand().HasInfo());
    if (!shown)
        return;

    SyncContent();
}

std::vector<CGrandShopWindow::SetEntry> mu::ui::window::CGrandShopWindow::BuildSets(const std::vector<Line>& lines)
{
    std::vector<SetEntry> sets;
    for (const auto& line : lines)
    {
        const std::wstring key = SetKeyOf(line);
        auto found = std::find_if(sets.begin(), sets.end(), [&key](const SetEntry& set) { return set.key == key; });
        if (found == sets.end())
        {
            sets.push_back({key, line.LineKind == Kind::Jewel ? std::wstring(I18N::Game::GrandShopJewelRow) : line.SetName,
                            line.LineKind == Kind::HighTier, {}});
            found = sets.end() - 1;
        }
        found->lines.push_back(&line);
    }

    // The Jewel of Ancient has its row last.
    std::stable_partition(sets.begin(), sets.end(), [](const SetEntry& set) { return set.key != JewelKey; });
    return sets;
}

const CGrandShopWindow::SetEntry* mu::ui::window::CGrandShopWindow::ShownSet(const std::vector<SetEntry>& sets)
{
    if (sets.empty())
        return nullptr;

    const auto picked = std::find_if(sets.begin(), sets.end(), [this](const SetEntry& set) { return set.key == m_pickedSet; });
    if (picked != sets.end())
        return &*picked;

    m_pickedSet = sets.front().key;
    return &sets.front();
}

void mu::ui::window::CGrandShopWindow::SyncContent()
{
    GrandShopRmlModel updated;
    updated.title = MakeLine(I18N::Game::GrandShopTitle, WINDOW_WIDTH, true);
    updated.note = MakeWrapped(I18N::Game::GrandShopBoundNote, MaxNoteLines);

    const auto& store = Grand();
    updated.hasInfo = store.HasInfo();
    std::vector<ShownIcon> icons;
    m_setKeys.clear();
    m_itemKeys.clear();
    if (updated.hasInfo)
    {
        updated.marks = MakeLine(Printf(I18N::Game::GrandShopMarks, Thousands(store.GetStatus().Marks).c_str()), CONTENT_WIDTH, true);
        updated.topLabel = MakeLine(I18N::Game::GrandShopTopMark, TOP_WIDTH, true);
        updated.buyLabel = MakeLine(I18N::Game::GrandShopBuy, BUY_WIDTH, true);
        const auto sets = BuildSets(store.Lines());
        const SetEntry* shown = ShownSet(sets);
        SyncSets(updated, sets);
        SyncItems(updated, shown, icons);
    }
    else if (store.FailText().empty())
    {
        const bool waiting = std::chrono::steady_clock::now() - m_openedAt < AnswerTimeout;
        updated.marks = MakeLine(waiting ? I18N::Game::GrandShopAsking : I18N::Game::GrandShopNoAnswer, CONTENT_WIDTH);
    }
    SyncFoot(updated);
    m_shownIcons = std::move(icons);
    PublishModel(updated);
}

void mu::ui::window::CGrandShopWindow::SyncSets(GrandShopRmlModel& updated, const std::vector<SetEntry>& sets)
{
    for (size_t i = 0; i < sets.size(); ++i)
    {
        const SetEntry& set = sets[i];
        m_setKeys.push_back(set.key);
        updated.sets.push_back({MakeLine(set.name, SET_NAME_WIDTH), set.top, static_cast<int>(i), set.key == m_pickedSet});
    }
}

void mu::ui::window::CGrandShopWindow::SyncItems(GrandShopRmlModel& updated, const SetEntry* shown,
                                                 std::vector<ShownIcon>& icons)
{
    if (shown == nullptr)
        return;

    for (size_t i = 0; i < shown->lines.size(); ++i)
    {
        const Line& line = *shown->lines[i];
        const std::wstring why = WhyNotBuyable(line);
        GrandShopItem item;
        item.id = "gs_icon_" + std::to_string(i);
        item.name = MakeLine(ItemName(line), ITEM_NAME_WIDTH, true);
        item.price = MakeLine(Printf(I18N::Game::GrandShopPrice, Thousands(line.Price).c_str()), ITEM_PRICE_WIDTH);
        item.index = static_cast<int>(i);
        item.canBuy = why.empty() && !Grand().IsBusy();
        item.hint = StringUtils::WideToNarrow(why.c_str());
        updated.items.push_back(std::move(item));
        m_itemKeys.push_back(line.Key);
        icons.push_back({updated.items.back().id, PieceItemType(line), line.SetType});
    }
}

void mu::ui::window::CGrandShopWindow::SyncFoot(GrandShopRmlModel& updated) const
{
    const auto& store = Grand();
    std::wstring text;
    int kind = HINT_INFO;
    if (!store.FailText().empty())
    {
        text = store.FailText();
        kind = HINT_BAD;
    }
    else if (store.IsBusy())
    {
        text = I18N::Game::GrandShopWorking;
    }
    else if (m_showAnswer && !store.LastAnswerText().empty())
    {
        text = store.LastAnswerText();
        kind = store.LastAnswerSucceeded() ? HINT_GOOD : HINT_BAD;
    }
    else if (store.HasInfo() && !store.GetStatus().SafeZone)
    {
        text = I18N::Game::GrandShopNotSafeZone;
        kind = HINT_BAD;
    }
    else if (store.HasInfo() && store.Lines().empty())
    {
        text = I18N::Game::GrandShopNoLines;
    }

    updated.hint = MakeWrapped(text, MaxHintLines);
    updated.hintKind = kind;
}

void mu::ui::window::CGrandShopWindow::PublishModel(GrandShopRmlModel& updated)
{
    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &GrandShopRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::marks, "marks", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::hasInfo, "has_info", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::sets, "sets", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::items, "items", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::topLabel, "top_label", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::buyLabel, "buy_label", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::hint, "hint", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::hintKind, "hint_kind", updated);
    SyncFieldFrom(binder, &GrandShopRmlModel::note, "note", updated);
}

void mu::ui::window::CGrandShopWindow::RenderItems(const Rml::Vector2f& offset, const Rml::Vector2f& size)
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr)
        return;

    for (const auto& icon : m_shownIcons)
    {
        Rml::Element* box = document->GetElementById(icon.id);
        Rml::Vector2f boxOffset;
        Rml::Vector2f boxSize;
        if (box == nullptr || !UI::RmlBridge::DrawnBox(*box, Rml::BoxArea::Border, boxOffset, boxSize))
            continue;
        // Only the boxes the pane shows (the target crops the ones it cuts).
        if (boxOffset.y + boxSize.y <= offset.y || boxOffset.y >= offset.y + size.y)
            continue;
        ::RenderItem3D(boxOffset.x, boxOffset.y, boxSize.x, boxSize.y, icon.type, 0, 0, icon.setType, false);
    }
}
