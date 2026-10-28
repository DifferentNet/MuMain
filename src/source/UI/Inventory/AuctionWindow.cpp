#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Inventory/AuctionWindow.h"

#include "Audio/DSPlaySound.h"
#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Items/ItemCombatPower.h"
#include "GameLogic/Items/JewelBank.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Inventory/ItemFromData.h"
#include "UI/Inventory/MyInventory.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Text/CUIRenderText.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlNumericInputFilter.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <functional>
#include <utility>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::Auction::Auctions;
using GameLogic::JewelBank::Bank;

namespace
{
// Raul the Auctioneer; farther from him than the add-on's NpcRange the server refuses, so the window closes.
constexpr int AuctioneerNpc = 546;
constexpr int MaxDistance = 6;
// Without an answer this long after asking, the server doesn't send it.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// The rows' time left is written again this often.
constexpr auto RowsRefresh = std::chrono::seconds(5);
// The longest name search, in characters (the add-on cuts it there too).
constexpr size_t MaxSearch = 40;
// A price's first line asks for Jewels of Bless at first (the jewel bank's kind 3).
constexpr int FirstKind = 3;

// The categories: the item groups the server's "cat=" takes (OpenMU's groups 0 to 15).
constexpr const wchar_t* CategoryGroups[] = {L"", L"0,1,2,3,4,5", L"6", L"7,8,9,10,11", L"12", L"13", L"14", L"15"};
constexpr int CategoryCount = static_cast<int>(std::size(CategoryGroups));
// The orders the server's "sort=" takes: newest first, ending first, by CP, by level, by name.
constexpr const wchar_t* SortKeys[] = {L"new", L"end", L"cp", L"lvl", L"name"};
constexpr int SortCount = static_cast<int>(std::size(SortKeys));

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

// 21450 -> "21,450".
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
AuctionLine Line(const std::wstring& text, int width, bool bold = false)
{
    const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
    // An empty line keeps its font size, so it keeps its height and the lines under it stay put.
    if (text.empty())
        return {{}, UI::RmlBridge::NativeTextPx(role)};
    g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
    return {StringUtils::WideToNarrow(text.c_str()),
            UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)), static_cast<float>(width))};
}

// The same, cut short with "..." when even the smallest size would overflow the box (a long name).
AuctionLine ClippedLine(const std::wstring& text, int width, bool bold = false)
{
    AuctionLine line = Line(text, width, bold);
    const float normal = UI::RmlBridge::NativeTextPx(bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal);
    if (text.empty() || normal <= 0.0f)
        return line;
    const float ratio = line.textPx / normal;
    if (static_cast<float>(MeasureInReferenceUnits(text)) * ratio <= static_cast<float>(width))
        return line;
    std::wstring cut = text;
    while (cut.size() > 1 && static_cast<float>(MeasureInReferenceUnits(cut + L"...")) * ratio > static_cast<float>(width))
        cut.pop_back();
    while (!cut.empty() && cut.back() == L' ')
        cut.pop_back();
    line.text = StringUtils::WideToNarrow((cut + L"...").c_str());
    return line;
}

// A text that wraps in its box instead of shrinking (the server's answers).
AuctionLine WrappedLine(const std::wstring& text)
{
    if (text.empty())
        return {};
    return {StringUtils::WideToNarrow(text.c_str()), UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal)};
}

AuctionButton Button(int id, const std::wstring& label, int width, bool on = false, bool off = false)
{
    AuctionButton button;
    button.id = id;
    button.label = Line(label, width - 4);
    button.on = on;
    button.off = off;
    return button;
}

// A typed text as one line for the server: control characters become spaces, the ends are trimmed, at most `max`.
std::wstring OneLine(std::wstring text, size_t max)
{
    for (auto& c : text)
    {
        if (std::iswcntrl(c))
            c = L' ';
    }
    const size_t first = text.find_first_not_of(L' ');
    if (first == std::wstring::npos)
        return {};
    text = text.substr(first, text.find_last_not_of(L' ') - first + 1);
    if (text.size() > max)
        text.resize(max);
    return text;
}

// How many inventory cells the item takes, across and down.
Rml::Vector2f ItemCells(int type)
{
    const auto* definition = g_ItemDatabase.Find(type);
    const float width = definition != nullptr && definition->width > 0 ? static_cast<float>(definition->width) : 1.f;
    const float height = definition != nullptr && definition->height > 0 ? static_cast<float>(definition->height) : 1.f;
    return {width, height};
}

// The item's name as its tooltip calls it: an ancient one by its set ("Hyon Gloves +13"); the level only for gear
// (groups 0 to 12; for the others GetItemName names the level itself: "Box of Kundun +5").
std::wstring ItemTitle(const ITEM* item)
{
    if (item == nullptr)
        return L"?";
    const int level = static_cast<int>(item->Level);
    const bool gear = item->Type / MAX_ITEM_INDEX <= 12;
    wchar_t name[256]{};
    if (item->AncientDiscriminator == 0 || !g_csItemOption.GetAncientItemName(name, std::size(name), item->Type, item->AncientDiscriminator))
        GetItemName(item->Type, level, name);
    return gear && level > 0 ? Printf(L"%ls +%d", name, level) : std::wstring(name);
}

bool IsExcellent(const ITEM* item)
{
    return item != nullptr && item->Type / MAX_ITEM_INDEX != 12 && (item->ExcellentFlags & 63) != 0;
}

bool IsAncient(const ITEM* item)
{
    return item != nullptr && item->AncientDiscriminator != 0;
}

int CombatPower(const ITEM* item)
{
    return item != nullptr ? GameLogic::Items::GetItemCombatPower(GameLogic::Items::DescribeForCombatPower(*item)) : 0;
}

std::wstring CategoryName(int category)
{
    const wchar_t* const names[] = {
        I18N::Game::AuctionCatAll,   I18N::Game::AuctionCatWeapons, I18N::Game::AuctionCatShields, I18N::Game::AuctionCatArmor,
        I18N::Game::AuctionCatWings, I18N::Game::AuctionCatJewelry, I18N::Game::AuctionCatOther,   I18N::Game::AuctionCatScrolls,
    };
    return names[category >= 0 && category < static_cast<int>(std::size(names)) ? category : 0];
}

std::wstring SortName(int sort)
{
    const wchar_t* const names[] = {
        I18N::Game::AuctionSortNew, I18N::Game::AuctionSortEnd, I18N::Game::AuctionSortCp, I18N::Game::AuctionSortLevel, I18N::Game::AuctionSortName,
    };
    return names[sort >= 0 && sort < static_cast<int>(std::size(names)) ? sort : 0];
}

// A jewel bank kind's short name ("Bless"); "-" for none.
std::wstring KindName(int kind)
{
    const wchar_t* const names[] = {
        I18N::Game::AuctionKindChaos,      I18N::Game::AuctionKindLife,        I18N::Game::AuctionKindSoul,    I18N::Game::AuctionKindBless,
        I18N::Game::AuctionKindCreation,   I18N::Game::AuctionKindGuardian,    I18N::Game::AuctionKindHarmony, I18N::Game::AuctionKindGemstone,
        I18N::Game::AuctionKindLowerStone, I18N::Game::AuctionKindHigherStone, I18N::Game::AuctionKindAncient, I18N::Game::AuctionKindFate,
    };
    static_assert(std::size(names) == GameLogic::JewelBank::KindCount);
    return kind >= 0 && kind < static_cast<int>(std::size(names)) ? std::wstring(names[kind]) : std::wstring(I18N::Game::AuctionNoKind);
}

// "5 Bless, 2 Soul".
std::wstring PriceText(const GameLogic::Auction::Price& price)
{
    std::wstring text;
    for (const auto& line : price)
    {
        if (!text.empty())
            text += L", ";
        text += std::to_wstring(line.count) + L" " + KindName(line.kind);
    }
    return text;
}

// A lot's terms as its row says them: "5 Bless", "bid 7 Soul", "5 Bless; bid 7 Soul" (the leading bid, else the
// start).
std::wstring LotTerms(const GameLogic::Auction::Lot& lot)
{
    const std::wstring buyout = PriceText(lot.price);
    if (!lot.TakesBids())
        return buyout;
    const int shown = lot.bidCount > 0 ? lot.bidCount : lot.nextBid;
    const std::wstring bid = Printf(I18N::Game::AuctionBidShort, shown, KindName(lot.bidKind).c_str());
    return buyout.empty() ? bid : buyout + L"; " + bid;
}

// What the jewel bank holds of a price: "Bless 3/5, Soul 7/2"; `missing` when a line has fewer than it asks.
std::wstring BankText(const GameLogic::Auction::Price& price, bool& missing)
{
    missing = false;
    std::wstring text;
    for (const auto& line : price)
    {
        const int held = Bank().Count(line.kind);
        missing = missing || held < line.count;
        if (!text.empty())
            text += L", ";
        text += KindName(line.kind) + L" " + std::to_wstring(held) + L"/" + std::to_wstring(line.count);
    }
    return text;
}

// The seconds a list's lot has left now: the server's count less the time since its answer came.
long long SecondsLeft(long long left, std::chrono::steady_clock::time_point receivedAt)
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - receivedAt).count();
    return std::max(0LL, left - static_cast<long long>(elapsed));
}

// "2d 5h", "5h 12m", "12m" (the last minute counts as one).
std::wstring TimeText(long long seconds)
{
    const int days = static_cast<int>(seconds / 86400);
    const int hours = static_cast<int>(seconds / 3600 % 24);
    const int minutes = static_cast<int>(seconds / 60 % 60);
    if (days > 0)
        return Printf(I18N::Game::AuctionLeftDays, days, hours);
    if (hours > 0)
        return Printf(I18N::Game::AuctionLeftHours, hours, minutes);
    return Printf(I18N::Game::AuctionLeftMinutes, static_cast<int>((seconds + 59) / 60));
}

// A lot of the player's own, or one it bid on, as its row says it: "On sale, 2d 5h left", "Bid 7 Soul, 2d 5h left",
// "Sold to Name", "Not sold: take it back", "Your bid leads, 5h 12m left", "Outbid: take the jewels back", "Won: take
// it".
std::wstring StateText(const GameLogic::Auction::OwnLot& lot, std::chrono::steady_clock::time_point receivedAt)
{
    using GameLogic::Auction::OwnLotState;
    const std::wstring left = TimeText(SecondsLeft(lot.secondsLeft, receivedAt));
    switch (lot.state)
    {
    case OwnLotState::Active:
        if (lot.bidCount > 0)
            return Printf(I18N::Game::AuctionStateBid, lot.bidCount, KindName(lot.bidKind).c_str(), left.c_str());
        return Printf(I18N::Game::AuctionStateActive, left.c_str());
    case OwnLotState::Sold:
        return Printf(I18N::Game::AuctionStateSold, lot.other.c_str());
    case OwnLotState::Unsold:
        return I18N::Game::AuctionStateUnsold;
    case OwnLotState::Leading:
        return Printf(I18N::Game::AuctionStateLeading, left.c_str());
    case OwnLotState::Outbid:
        return I18N::Game::AuctionStateOutbid;
    case OwnLotState::Won:
        return I18N::Game::AuctionStateWon;
    default:
        return I18N::Game::AuctionStateBusy;
    }
}

// A count typed in a field: 0 when it's empty or too long.
long long TypedCount(const Rml::String& text)
{
    const Rml::String digits = UI::RmlBridge::KeepDigitsOnly(text);
    return digits.empty() || digits.size() > 9 ? 0 : std::atoll(digits.c_str());
}

// A "find" / "mine" answer is awaited while the list's version is still the one it had when asked.
AuctionLine WaitLine(std::chrono::steady_clock::time_point askedAt, int width)
{
    const bool waiting = std::chrono::steady_clock::now() - askedAt < AnswerTimeout;
    return Line(waiting ? I18N::Game::AuctionAsking : I18N::Game::AuctionNoAnswer, width);
}

// The bag's items now (the inventory's main grid), by slot as the server counts it (12 is the first).
struct BagEntry
{
    int slot = 0;
    ITEM* item = nullptr;
};

std::vector<BagEntry> CurrentBag()
{
    std::vector<BagEntry> bag;
    CInventoryCtrl* grid = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
    if (grid == nullptr)
        return bag;
    const size_t count = grid->GetNumberOfItems();
    for (size_t i = 0; i < count; ++i)
    {
        ITEM* item = grid->GetItem(static_cast<int>(i));
        // The item's slot number: GetIndexByItem counts the grid's offset (12) in already.
        const int slot = grid->GetIndexByItem(item);
        if (item != nullptr && slot >= 0)
            bag.push_back({slot, item});
    }
    std::sort(bag.begin(), bag.end(), [](const BagEntry& a, const BagEntry& b) { return a.slot < b.slot; });
    return bag;
}

std::array<int, 6> KeyOf(int slot, const ITEM& item)
{
    return {slot, static_cast<int>(item.Type), static_cast<int>(item.Level), static_cast<int>(item.Durability),
            static_cast<int>(item.ExcellentFlags), static_cast<int>(item.AncientDiscriminator)};
}

// The bag's item at `slot` is still the one the key was made from.
bool BagStillHas(const std::array<int, 6>& key)
{
    for (const auto& entry : CurrentBag())
    {
        if (entry.slot == key[0])
            return KeyOf(entry.slot, *entry.item) == key;
    }
    return false;
}

void Confirm(std::vector<GenericDialogConfig::Line> lines, const ITEM* item, std::function<void()> onYes, const char* tag)
{
    GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.lines = std::move(lines);
    if (item != nullptr)
        cfg.item3D = *item;
    cfg.onPrimary = std::move(onYes);
    cfg.tag = tag;
    g_pGenericConfirmDialog->Show(std::move(cfg));
}
} // namespace

mu::ui::window::CAuctionWindow::CAuctionWindow() = default;

mu::ui::window::CAuctionWindow::~CAuctionWindow()
{
    Release();
}

bool mu::ui::window::CAuctionWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_AUCTION, this);
    // The price's counts take digits only; a theme switch builds the document again.
    m_RmlView.SetAfterBuild([this] { UI::RmlBridge::AttachNumericInputFilter(m_RmlView.Document()); });
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CAuctionWindow::Release()
{
    m_itemTarget.Disable();
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CAuctionWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CAuctionWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CAuctionWindow::OpenningProcess()
{
    auto& store = Auctions();
    m_seenResult = store.ResultVersion();
    m_seenSold = store.SoldVersion();
    m_status.clear();
    m_statusBad = false;
    // Raul's talk sends the settings and the jewel bank's counts; the window opened otherwise asks for them.
    if (!store.HasSettings())
        store.RequestSettings();
    if (!Bank().HasInfo())
        Bank().Request();
    m_rowsBuiltAt = {};
    RequestCurrent();
}

void mu::ui::window::CAuctionWindow::ClosingProcess()
{
    if (Rml::ElementDocument* document = m_RmlView.Document())
    {
        if (Rml::Element* focused = document->GetFocusLeafNode(); focused != nullptr && focused != document)
            focused->Blur();
    }
}

bool mu::ui::window::CAuctionWindow::UpdateMouseEvent()
{
    // Everything in the window is RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CAuctionWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_AUCTION))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_AUCTION);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    // Enter in the name field searches.
    Rml::ElementDocument* document = m_RmlView.Document();
    const bool typing = document != nullptr && RmlUiRuntime::Instance().GetKeyOwningDocument() == document;
    if (typing && IsPress(VK_RETURN))
    {
        Rml::Element* focused = document->GetFocusLeafNode();
        if (focused != nullptr && focused->GetId() == "au_search")
            m_pendingFind = true;
        else if (focused != nullptr && focused->GetId() == "au_bid")
            m_pendingAction = ACT_BID;
        return false;
    }

    return true;
}

bool mu::ui::window::CAuctionWindow::Update()
{
    if (IsVisible() && !CloseWhenAway())
    {
        HandleClicks();
        TakeResult();

        // A lot sold while the window is open: the own lots and the count on sale changed.
        auto& store = Auctions();
        if (IsVisible() && store.SoldVersion() != m_seenSold)
        {
            m_seenSold = store.SoldVersion();
            store.RequestSettings();
            if (m_tab == TAB_MINE)
                RequestMine();
            else if (m_tab == TAB_SEARCH)
                Search(store.Found().page);
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CAuctionWindow::Render()
{
    // The frame, the lists and the buttons are RmlUi; the picked item's icon is drawn by m_itemTarget. Only an
    // item's tooltip is native: drawn every frame while the icon or the item's row is hovered (it hides itself in a
    // frame it isn't drawn).
    if (!IsVisible())
        return true;

    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (ITEM* item = ShownItem())
    {
        Rml::Element* icon = GetField("au_icon");
        if (icon != nullptr && icon->IsPseudoClassSet("hover") && UI::RmlBridge::DrawnBox(*icon, Rml::BoxArea::Border, offset, size))
        {
            ::RenderItemInfoAtPx(offset.x + size.x / 2.f, offset.y + size.y, item, false);
            return true;
        }
    }

    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr)
        return true;
    Rml::ElementList rows;
    document->QuerySelectorAll(rows, ".au-row");
    for (Rml::Element* row : rows)
    {
        if (!row->IsPseudoClassSet("hover"))
            continue;
        ITEM* item = RowItem(row->GetAttribute<int>("au-row-index", -1));
        if (item != nullptr && UI::RmlBridge::DrawnBox(*row, Rml::BoxArea::Border, offset, size))
            ::RenderItemInfoAtPx(offset.x + size.x / 2.f, offset.y + size.y, item, false);
        break;
    }
    return true;
}

bool mu::ui::window::CAuctionWindow::CloseWhenAway()
{
    bool away = Hero == nullptr || !g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_INVENTORY);
    if (!away)
    {
        const int index = FindCharacterIndexByMonsterIndex(AuctioneerNpc);
        if (index < 0 || index >= MAX_CHARACTERS_CLIENT)
        {
            away = true;
        }
        else
        {
            const CHARACTER& npc = CharactersClient[index];
            const int dx = std::abs(npc.PositionX - Hero->PositionX);
            const int dy = std::abs(npc.PositionY - Hero->PositionY);
            away = std::max(dx, dy) > MaxDistance;
        }
    }
    if (away)
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_AUCTION);
    return away;
}

void mu::ui::window::CAuctionWindow::HandleClicks()
{
    const int tab = std::exchange(m_pendingTab, -1);
    const int action = std::exchange(m_pendingAction, 0);
    const int row = std::exchange(m_pendingRow, -1);
    const int days = std::exchange(m_pendingDays, -1);
    const int kindLine = std::exchange(m_pendingKindLine, -1);
    const int kindStep = std::exchange(m_pendingKindStep, 0);
    const bool find = std::exchange(m_pendingFind, false);
    const bool exit = std::exchange(m_pendingExit, false);
    if (!IsVisible())
        return;

    if (exit)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_AUCTION);
        PlayBuffer(SOUND_CLICK01);
        return;
    }

    if (tab >= 0 || action > 0 || row >= 0 || days >= 0 || kindLine >= 0 || find)
        PlayBuffer(SOUND_CLICK01);

    if (tab >= 0 && tab != m_tab)
        SwitchTab(tab);
    if (row >= 0)
        PickRow(row);
    if (days >= 0 && m_tab == TAB_SELL && days < static_cast<int>(Auctions().GetSettings().durations.size()))
        m_days = days;
    if (kindLine >= 0 && kindLine <= BidLine && m_tab == TAB_SELL)
        StepKind(kindLine, kindStep);
    if (find && m_tab == TAB_SEARCH)
        Search(1);
    if (action > 0)
        DoAction(action);
}

void mu::ui::window::CAuctionWindow::SwitchTab(int tab)
{
    if (tab < TAB_SEARCH || tab > TAB_SELL)
        return;
    m_tab = tab;
    m_rowsBuiltAt = {};
    RequestCurrent();
}

void mu::ui::window::CAuctionWindow::PickRow(int row)
{
    if (m_tab == TAB_SEARCH)
    {
        const auto& lots = Auctions().Found().lots;
        if (m_foundSeen == Auctions().FoundVersion() && row >= 0 && row < static_cast<int>(lots.size()))
            m_searchPicked = lots[static_cast<size_t>(row)].number;
    }
    else if (m_tab == TAB_MINE)
    {
        const auto& lots = Auctions().Mine().lots;
        if (m_mineSeen == Auctions().MineVersion() && row >= 0 && row < static_cast<int>(lots.size()))
            m_minePicked = lots[static_cast<size_t>(row)].number;
    }
    else if (row >= 0 && row < static_cast<int>(m_bag.size()))
    {
        m_sellSlot = m_bag[static_cast<size_t>(row)].slot;
    }
}

void mu::ui::window::CAuctionWindow::DoAction(int action)
{
    // A switched-off button still reports its click: each action checks it may run.
    if (m_tab == TAB_SEARCH)
    {
        const auto& found = Auctions().Found();
        switch (action)
        {
        case ACT_CATEGORY:
            m_category = (m_category + 1) % CategoryCount;
            Search(1);
            break;
        case ACT_CLASS:
            m_myClass = !m_myClass;
            Search(1);
            break;
        case ACT_EXCELLENT:
            m_excellent = !m_excellent;
            Search(1);
            break;
        case ACT_ANCIENT:
            m_ancient = !m_ancient;
            Search(1);
            break;
        case ACT_SORT:
            m_sort = (m_sort + 1) % SortCount;
            Search(1);
            break;
        case ACT_PREVIOUS:
            if (found.page > 1)
                Search(found.page - 1);
            break;
        case ACT_NEXT:
            if (found.page < found.pages)
                Search(found.page + 1);
            break;
        case ACT_BUY:
            Buy();
            break;
        case ACT_BID:
            Bid();
            break;
        default:
            break;
        }
    }
    else if (m_tab == TAB_MINE)
    {
        switch (action)
        {
        case ACT_CANCEL:
            CancelPicked();
            break;
        case ACT_TAKE:
            Take(false);
            break;
        case ACT_TAKE_ALL:
            Take(true);
            break;
        case ACT_REFRESH:
            RequestMine();
            break;
        default:
            break;
        }
    }
    else if (action == ACT_SELL)
    {
        Sell();
    }
    else if (action == ACT_CLEAR)
    {
        ClearSell();
    }
}

void mu::ui::window::CAuctionWindow::StepKind(int line, int step)
{
    const auto& settings = Auctions().GetSettings();
    const bool bid = line == BidLine;
    int& current = bid ? m_bidKind : m_priceKinds[static_cast<size_t>(line)];
    // The choices: none, then the kinds the server takes; for a price line those no other price line has.
    std::vector<int> choices{NoKind};
    for (int kind = 0; kind < GameLogic::JewelBank::KindCount; ++kind)
    {
        bool taken = false;
        for (int other = 0; other < PriceLineSlots && !bid; ++other)
            taken = taken || (other != line && m_priceKinds[static_cast<size_t>(other)] == kind);
        if (settings.Allows(kind) && !taken)
            choices.push_back(kind);
    }

    const auto at = std::find(choices.begin(), choices.end(), current);
    const int count = static_cast<int>(choices.size());
    int index = at == choices.end() ? 0 : static_cast<int>(at - choices.begin()) + (step < 0 ? -1 : 1);
    index = (index % count + count) % count;
    current = choices[static_cast<size_t>(index)];
}

void mu::ui::window::CAuctionWindow::TakeResult()
{
    auto& store = Auctions();
    if (store.ResultVersion() == m_seenResult)
        return;
    m_seenResult = store.ResultVersion();

    const auto& result = store.LastResult();
    SetStatus(result.text, !result.ok);
    if (result.op == L"buy")
    {
        if (result.ok)
        {
            m_searchPicked = -1;
            Bank().Request();
        }
        Search(store.Found().page);
    }
    else if (result.op == L"bid")
    {
        // Refused or not, the lot's bids changed (someone may have bid first): its page again. The server sends
        // the jewel bank's counts itself.
        Search(store.Found().page);
    }
    else if (result.op == L"sell")
    {
        if (result.ok)
            m_sellSlot = -1;
        store.RequestSettings();
    }
    else if (result.op == L"cancel")
    {
        RequestMine();
    }
    else if (result.op == L"take")
    {
        if (result.ok)
            Bank().Request();
        RequestMine();
    }
    else if (result.op == L"find" && !result.ok)
    {
        // Refused (too far, too fast): no page comes.
        m_foundAsked = -1;
    }
    else if (result.op == L"mine" && !result.ok)
    {
        m_mineAsked = -1;
    }
}

void mu::ui::window::CAuctionWindow::RequestCurrent()
{
    if (m_tab == TAB_SEARCH)
        Search(m_page);
    else if (m_tab == TAB_MINE)
        RequestMine();
    else
        Auctions().RequestSettings();
}

void mu::ui::window::CAuctionWindow::Search(int page)
{
    m_page = std::max(1, page);
    std::wstring request = L"find";
    if (m_category > 0 && m_category < CategoryCount)
        request += std::wstring(L" cat=") + CategoryGroups[m_category];
    if (m_myClass)
        request += L" cls=mine";
    if (m_excellent)
        request += L" exc=1";
    if (m_ancient)
        request += L" anc=1";
    request += std::wstring(L" sort=") + SortKeys[std::clamp(m_sort, 0, SortCount - 1)] + L" page=" + std::to_wstring(m_page);
    // The name comes last: the server takes the rest of the line.
    const std::wstring text = OneLine(StringUtils::NarrowToWide(m_RmlView.GetModel().searchText), MaxSearch);
    if (!text.empty())
        request += L" q=" + text;

    m_foundAsked = Auctions().FoundVersion();
    m_requestedAt = std::chrono::steady_clock::now();
    Auctions().Send(request);
}

void mu::ui::window::CAuctionWindow::RequestMine()
{
    m_mineAsked = Auctions().MineVersion();
    m_requestedAt = std::chrono::steady_clock::now();
    Auctions().Send(L"mine");
}

void mu::ui::window::CAuctionWindow::Buy()
{
    const int picked = FoundIndex();
    if (picked < 0)
        return;
    const auto& lot = Auctions().Found().lots[static_cast<size_t>(picked)];
    if (lot.own || lot.price.empty())
        return;

    const ITEM* item = m_foundItems[static_cast<size_t>(picked)].get();
    const std::wstring request = L"buy " + std::to_wstring(lot.number) + L" " + std::to_wstring(lot.version);
    Confirm({{Printf(I18N::Game::AuctionBuyConfirm, ItemTitle(item).c_str(), lot.seller.c_str()), true},
             {Printf(I18N::Game::AuctionPrice, PriceText(lot.price).c_str()), false},
             {I18N::Game::AuctionBuyFromBank, false}},
            item, [request] { Auctions().Send(request); }, "auction_buy");
}

void mu::ui::window::CAuctionWindow::Bid()
{
    const int picked = FoundIndex();
    if (picked < 0)
        return;
    const auto& settings = Auctions().GetSettings();
    const auto& lot = Auctions().Found().lots[static_cast<size_t>(picked)];
    if (lot.own || !lot.TakesBids() || lot.leading)
        return;

    const long long count = TypedCount(m_RmlView.GetModel().bidText);
    const int most = settings.maximumCount > 0 ? settings.maximumCount : 9999;
    if (count < std::max(1, lot.nextBid) || count > most)
    {
        SetStatus(Printf(I18N::Game::AuctionNeedBid, std::max(1, lot.nextBid), most), true);
        return;
    }

    // The jewels leave the bank for the bid until it is beaten; the question says what the bank holds of them.
    const ITEM* item = m_foundItems[static_cast<size_t>(picked)].get();
    const int bid = static_cast<int>(count);
    const std::wstring request = L"bid " + std::to_wstring(lot.number) + L" " + std::to_wstring(bid);
    std::vector<GenericDialogConfig::Line> lines{
        {Printf(I18N::Game::AuctionBidConfirm, bid, KindName(lot.bidKind).c_str(), ItemTitle(item).c_str()), true},
        {I18N::Game::AuctionBidHold, false},
    };
    if (Bank().HasInfo())
    {
        bool missing = false;
        lines.push_back({Printf(I18N::Game::AuctionYourBank, BankText({{lot.bidKind, bid}}, missing).c_str()), false});
    }
    if (settings.bidExtendMinutes > 0)
        lines.push_back({Printf(I18N::Game::AuctionBidExtend, settings.bidExtendMinutes, settings.bidExtendMinutes), false});
    Confirm(std::move(lines), item, [request] { Auctions().Send(request); }, "auction_bid");
}

void mu::ui::window::CAuctionWindow::CancelPicked()
{
    const int picked = MineIndex();
    if (picked < 0)
        return;
    const auto& lot = Auctions().Mine().lots[static_cast<size_t>(picked)];
    if (lot.bidder || lot.state != GameLogic::Auction::OwnLotState::Active || lot.bidCount > 0)
        return;

    const ITEM* item = m_mineItems[static_cast<size_t>(picked)].get();
    const std::wstring request = L"cancel " + std::to_wstring(lot.number);
    Confirm({{Printf(I18N::Game::AuctionCancelConfirm, ItemTitle(item).c_str()), true}}, item,
            [request] { Auctions().Send(request); }, "auction_cancel");
}

void mu::ui::window::CAuctionWindow::Take(bool all)
{
    const auto& lots = Auctions().Mine().lots;
    if (all)
    {
        if (std::any_of(lots.begin(), lots.end(), [](const auto& lot) { return lot.Takeable(); }))
            Auctions().Send(L"take all");
        return;
    }

    const int picked = MineIndex();
    if (picked < 0)
        return;
    const auto& lot = lots[static_cast<size_t>(picked)];
    if (lot.Takeable())
        Auctions().Send(L"take " + std::to_wstring(lot.number));
}

bool mu::ui::window::CAuctionWindow::TypedPrice(GameLogic::Auction::Price& price)
{
    const auto& settings = Auctions().GetSettings();
    const auto& model = m_RmlView.GetModel();
    const Rml::String* counts[PriceLineSlots] = {&model.count0Text, &model.count1Text, &model.count2Text};
    const int lines = std::clamp(settings.priceLines, 1, PriceLineSlots);
    const int most = settings.maximumCount > 0 ? settings.maximumCount : 9999;

    price.clear();
    for (int i = 0; i < lines; ++i)
    {
        const int kind = m_priceKinds[static_cast<size_t>(i)];
        if (kind < 0)
            continue;
        const long long count = TypedCount(*counts[i]);
        if (count < 1 || count > most)
        {
            SetStatus(Printf(I18N::Game::AuctionNeedCount, most), true);
            return false;
        }
        if (std::any_of(price.begin(), price.end(), [kind](const auto& line) { return line.kind == kind; }))
        {
            SetStatus(I18N::Game::AuctionKindTwice, true);
            return false;
        }
        price.push_back({kind, static_cast<int>(count)});
    }
    return true;
}

bool mu::ui::window::CAuctionWindow::TypedBid(int& kind, int& start)
{
    const auto& settings = Auctions().GetSettings();
    kind = m_bidKind;
    start = 0;
    if (kind < 0)
        return true;
    const int most = settings.maximumCount > 0 ? settings.maximumCount : 9999;
    const long long count = TypedCount(m_RmlView.GetModel().bidStartText);
    if (count < 1 || count > most)
    {
        SetStatus(Printf(I18N::Game::AuctionNeedCount, most), true);
        return false;
    }
    start = static_cast<int>(count);
    return true;
}

void mu::ui::window::CAuctionWindow::Sell()
{
    const auto& settings = Auctions().GetSettings();
    const int index = BagIndex();
    if (index < 0)
    {
        SetStatus(I18N::Game::AuctionPickItem, true);
        return;
    }
    if (!Auctions().HasSettings() || settings.durations.empty())
        return;
    GameLogic::Auction::Price price;
    int bidKind = NoKind;
    int bidStart = 0;
    if (!TypedPrice(price) || !TypedBid(bidKind, bidStart))
        return;
    if (price.empty() && bidKind < 0)
    {
        SetStatus(I18N::Game::AuctionNeedPrice, true);
        return;
    }

    // "sell slot days kind:count,... [bid=kind:start]", the price "-" without a buyout.
    const auto& entry = m_bag[static_cast<size_t>(index)];
    const int days = settings.durations[static_cast<size_t>(std::clamp(m_days, 0, static_cast<int>(settings.durations.size()) - 1))];
    std::wstring request = L"sell " + std::to_wstring(entry.slot) + L" " + std::to_wstring(days) + L" ";
    for (size_t i = 0; i < price.size(); ++i)
        request += (i > 0 ? L"," : L"") + std::to_wstring(price[i].kind + 1) + L":" + std::to_wstring(price[i].count);
    if (price.empty())
        request += L"-";
    if (bidKind >= 0)
        request += L" bid=" + std::to_wstring(bidKind + 1) + L":" + std::to_wstring(bidStart);

    // The item may move while the question is open: what is sold is what the slot holds when the answer comes.
    const auto key = KeyOf(entry.slot, *entry.item);
    std::vector<GenericDialogConfig::Line> lines{{Printf(I18N::Game::AuctionSellConfirm, ItemTitle(entry.item.get()).c_str(), days), true}};
    lines.push_back({price.empty() ? std::wstring(I18N::Game::AuctionSellNoBuyout) : Printf(I18N::Game::AuctionPrice, PriceText(price).c_str()), false});
    if (bidKind >= 0)
        lines.push_back({Printf(I18N::Game::AuctionSellBid, bidStart, KindName(bidKind).c_str()), false});
    lines.push_back({Printf(I18N::Game::AuctionSellFee, Thousands(settings.feePerDay * days).c_str()), false});
    Confirm(std::move(lines),
            entry.item.get(),
            [request, key]
            {
                if (BagStillHas(key))
                    Auctions().Send(request);
            },
            "auction_sell");
}

void mu::ui::window::CAuctionWindow::ClearSell()
{
    m_sellSlot = -1;
    m_priceKinds = {UnsetKind, NoKind, NoKind};
    m_bidKind = NoKind;
    auto& binder = m_RmlView.Binder();
    SyncField(binder, &AuctionRmlModel::count0Text, "count0_text", Rml::String("1"));
    SyncField(binder, &AuctionRmlModel::count1Text, "count1_text", Rml::String());
    SyncField(binder, &AuctionRmlModel::count2Text, "count2_text", Rml::String());
    SyncField(binder, &AuctionRmlModel::bidStartText, "bid_start_text", Rml::String());
    m_status.clear();
    m_statusBad = false;
}

void mu::ui::window::CAuctionWindow::TakeFound()
{
    const auto& store = Auctions();
    const auto& found = store.Found();
    const bool changed = m_foundSeen != store.FoundVersion();
    if (changed)
    {
        m_foundSeen = store.FoundVersion();
        m_foundItems.clear();
        for (const auto& lot : found.lots)
            m_foundItems.push_back(UI::Items::FromData::Make(lot.item));
        m_page = std::max(1, found.page);
        if (FoundIndex() < 0)
            m_searchPicked = -1;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!changed && now - m_rowsBuiltAt < RowsRefresh)
        return;
    m_rowsBuiltAt = now;

    m_foundRows.clear();
    for (size_t i = 0; i < found.lots.size(); ++i)
    {
        const auto& lot = found.lots[i];
        const ITEM* item = m_foundItems[i].get();
        AuctionRow row;
        row.index = static_cast<int>(i);
        row.title = ClippedLine(ItemTitle(item), ROW_WIDTH, true);
        row.detail = ClippedLine(LotTerms(lot) + L"  " + TimeText(SecondsLeft(lot.secondsLeft, found.receivedAt)), ROW_WIDTH);
        row.excellent = IsExcellent(item);
        row.ancient = IsAncient(item);
        row.dim = lot.own;
        row.live = lot.leading;
        m_foundRows.push_back(std::move(row));
    }
}

void mu::ui::window::CAuctionWindow::TakeMine()
{
    using GameLogic::Auction::OwnLotState;
    const auto& store = Auctions();
    const auto& mine = store.Mine();
    const bool changed = m_mineSeen != store.MineVersion();
    if (changed)
    {
        m_mineSeen = store.MineVersion();
        m_mineItems.clear();
        for (const auto& lot : mine.lots)
            m_mineItems.push_back(UI::Items::FromData::Make(lot.item));
        if (MineIndex() < 0)
            m_minePicked = -1;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!changed && now - m_rowsBuiltAt < RowsRefresh)
        return;
    m_rowsBuiltAt = now;

    m_mineRows.clear();
    for (size_t i = 0; i < mine.lots.size(); ++i)
    {
        const auto& lot = mine.lots[i];
        const ITEM* item = m_mineItems[i].get();
        AuctionRow row;
        row.index = static_cast<int>(i);
        row.title = ClippedLine(ItemTitle(item), ROW_WIDTH, true);
        row.detail = ClippedLine(StateText(lot, mine.receivedAt), ROW_WIDTH);
        row.excellent = IsExcellent(item);
        row.ancient = IsAncient(item);
        row.live = lot.Takeable();
        row.dim = lot.state == OwnLotState::Busy;
        m_mineRows.push_back(std::move(row));
    }
}

void mu::ui::window::CAuctionWindow::TakeBag()
{
    std::vector<BagKey> keys;
    const std::vector<BagEntry> bag = CurrentBag();
    for (const auto& entry : bag)
        keys.push_back(KeyOf(entry.slot, *entry.item));
    if (keys == m_bagKeys)
        return;

    // The picked item moved or changed (it went on sale): nothing is picked.
    if (m_sellSlot >= 0)
    {
        const auto was = std::find_if(m_bagKeys.begin(), m_bagKeys.end(), [this](const BagKey& key) { return key[0] == m_sellSlot; });
        const auto now = std::find_if(keys.begin(), keys.end(), [this](const BagKey& key) { return key[0] == m_sellSlot; });
        if (was == m_bagKeys.end() || now == keys.end() || *was != *now)
            m_sellSlot = -1;
    }

    m_bagKeys = std::move(keys);
    m_bag.clear();
    m_bagRows.clear();
    for (const auto& entry : bag)
    {
        // A copy: the bag's own item may go away under the list.
        BagItem copy;
        copy.slot = entry.slot;
        copy.item = std::make_unique<ITEM>(*entry.item);
        AuctionRow row;
        row.index = static_cast<int>(m_bag.size());
        row.title = ClippedLine(ItemTitle(copy.item.get()), ROW_WIDTH);
        row.excellent = IsExcellent(copy.item.get());
        row.ancient = IsAncient(copy.item.get());
        m_bag.push_back(std::move(copy));
        m_bagRows.push_back(std::move(row));
    }
}

int mu::ui::window::CAuctionWindow::FoundIndex() const
{
    if (m_searchPicked < 0 || m_foundSeen != Auctions().FoundVersion())
        return -1;
    const auto& lots = Auctions().Found().lots;
    for (size_t i = 0; i < lots.size() && i < m_foundItems.size(); ++i)
    {
        if (lots[i].number == m_searchPicked)
            return static_cast<int>(i);
    }
    return -1;
}

int mu::ui::window::CAuctionWindow::MineIndex() const
{
    if (m_minePicked < 0 || m_mineSeen != Auctions().MineVersion())
        return -1;
    const auto& lots = Auctions().Mine().lots;
    for (size_t i = 0; i < lots.size() && i < m_mineItems.size(); ++i)
    {
        if (lots[i].number == m_minePicked)
            return static_cast<int>(i);
    }
    return -1;
}

int mu::ui::window::CAuctionWindow::BagIndex() const
{
    if (m_sellSlot < 0)
        return -1;
    for (size_t i = 0; i < m_bag.size(); ++i)
    {
        if (m_bag[i].slot == m_sellSlot)
            return static_cast<int>(i);
    }
    return -1;
}

ITEM* mu::ui::window::CAuctionWindow::RowItem(int index) const
{
    if (index < 0)
        return nullptr;
    const auto at = static_cast<size_t>(index);
    if (m_tab == TAB_SEARCH)
        return m_foundSeen == Auctions().FoundVersion() && at < m_foundItems.size() ? m_foundItems[at].get() : nullptr;
    if (m_tab == TAB_MINE)
        return m_mineSeen == Auctions().MineVersion() && at < m_mineItems.size() ? m_mineItems[at].get() : nullptr;
    return at < m_bag.size() ? m_bag[at].item.get() : nullptr;
}

ITEM* mu::ui::window::CAuctionWindow::ShownItem() const
{
    if (m_tab == TAB_SEARCH)
        return RowItem(FoundIndex());
    if (m_tab == TAB_MINE)
        return RowItem(MineIndex());
    return RowItem(BagIndex());
}

float mu::ui::window::CAuctionWindow::ItemFieldOfView() const
{
    const ITEM* item = ShownItem();
    Rml::Element* icon = GetField("au_icon");
    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (item == nullptr || icon == nullptr || !UI::RmlBridge::DrawnContentBox(*icon, offset, size))
        return 1.f;

    // At a field of view of 1 the item camera draws an inventory cell about a 24th of the window high (20 px of
    // 480); a wider view draws it smaller, so the item's cells fill the icon's box.
    const Rml::Vector2f cells = ItemCells(item->Type);
    const float cellPx = std::min(size.x / cells.x, size.y / cells.y);
    const float nativeCellPx = static_cast<float>(WindowHeight) / 24.f;
    return std::clamp(nativeCellPx / std::max(cellPx, 1.f), 0.5f, 4.f);
}

void mu::ui::window::CAuctionWindow::RenderItems()
{
    const ITEM* item = IsVisible() ? ShownItem() : nullptr;
    if (item == nullptr)
        return;

    Rml::Element* icon = GetField("au_icon");
    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (icon == nullptr || !UI::RmlBridge::DrawnContentBox(*icon, offset, size))
        return;

    // The item's own box of cells, as big as fits and centred in the icon's (the model's anchor is a part of it).
    const Rml::Vector2f cells = ItemCells(item->Type);
    const float cell = std::min(size.x / cells.x, size.y / cells.y);
    const Rml::Vector2f box{cell * cells.x, cell * cells.y};
    ::RenderItem3D(offset.x + (size.x - box.x) / 2.f, offset.y + (size.y - box.y) / 2.f, box.x, box.y, item->Type, item->Level,
                   item->ExcellentFlags, item->AncientDiscriminator, false);
}

void mu::ui::window::CAuctionWindow::SetStatus(const std::wstring& text, bool bad)
{
    m_status = text;
    m_statusBad = bad;
}

Rml::Element* mu::ui::window::CAuctionWindow::GetField(const char* id) const
{
    Rml::ElementDocument* document = m_RmlView.Document();
    return document != nullptr ? document->GetElementById(id) : nullptr;
}

void mu::ui::window::CAuctionWindow::BindRmlModel(Rml::DataModelConstructor& c, AuctionRmlModel& model)
{
    auto lineType = c.RegisterStruct<AuctionLine>();
    lineType.RegisterMember("text", &AuctionLine::text);
    lineType.RegisterMember("text_px", &AuctionLine::textPx);

    auto buttonType = c.RegisterStruct<AuctionButton>();
    buttonType.RegisterMember("id", &AuctionButton::id);
    buttonType.RegisterMember("label", &AuctionButton::label);
    buttonType.RegisterMember("on", &AuctionButton::on);
    buttonType.RegisterMember("off", &AuctionButton::off);
    c.RegisterArray<std::vector<AuctionButton>>();

    auto rowType = c.RegisterStruct<AuctionRow>();
    rowType.RegisterMember("index", &AuctionRow::index);
    rowType.RegisterMember("title", &AuctionRow::title);
    rowType.RegisterMember("detail", &AuctionRow::detail);
    rowType.RegisterMember("selected", &AuctionRow::selected);
    rowType.RegisterMember("excellent", &AuctionRow::excellent);
    rowType.RegisterMember("ancient", &AuctionRow::ancient);
    rowType.RegisterMember("dim", &AuctionRow::dim);
    rowType.RegisterMember("live", &AuctionRow::live);
    c.RegisterArray<std::vector<AuctionRow>>();

    auto stepType = c.RegisterStruct<AuctionPriceStep>();
    stepType.RegisterMember("kind", &AuctionPriceStep::kind);
    stepType.RegisterMember("used", &AuctionPriceStep::used);

    c.Bind("title", &model.title);
    c.Bind("tab", &model.tab);
    c.Bind("tabs", &model.tabs);
    c.Bind("field_px", &model.fieldPx);

    c.Bind("filters", &model.filters);
    c.Bind("toggles", &model.toggles);
    c.Bind("search_text", &model.searchText);
    c.Bind("find", &model.find);

    c.Bind("list_info", &model.listInfo);
    c.Bind("rows", &model.rows);
    c.Bind("item_name", &model.itemName);
    c.Bind("sel_first", &model.selFirst);
    c.Bind("sel_second", &model.selSecond);
    c.Bind("sel_third", &model.selThird);
    c.Bind("sel_fourth", &model.selFourth);
    c.Bind("sel_alert", &model.selAlert);
    c.Bind("actions", &model.actions);
    c.Bind("more_actions", &model.moreActions);
    c.Bind("bid_text", &model.bidText);
    c.Bind("bid_open", &model.bidOpen);

    c.Bind("days", &model.days);
    c.Bind("price_line_count", &model.priceLineCount);
    c.Bind("price0", &model.price0);
    c.Bind("price1", &model.price1);
    c.Bind("price2", &model.price2);
    c.Bind("count0_text", &model.count0Text);
    c.Bind("count1_text", &model.count1Text);
    c.Bind("count2_text", &model.count2Text);
    c.Bind("bid_step", &model.bidStep);
    c.Bind("bid_start_text", &model.bidStartText);
    c.Bind("fee", &model.fee);
    // One of the first jewel unless the seller types more.
    if (model.count0Text.empty())
        model.count0Text = "1";

    c.Bind("status", &model.status);
    c.Bind("status_bad", &model.statusBad);
    c.Bind("exit_tooltip", &model.exitTooltip);

    auto intArg = [](const Rml::VariantList& args) { return args.size() == 1 ? args[0].Get<int>(-1) : -1; };
    c.BindEventCallback("au_tab", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingTab = intArg(args); });
    c.BindEventCallback("au_action", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingAction = std::max(0, intArg(args)); });
    c.BindEventCallback("au_row", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingRow = intArg(args); });
    c.BindEventCallback("au_days", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingDays = intArg(args); });
    c.BindEventCallback("au_kind_down", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            m_pendingKindLine = intArg(args);
                            m_pendingKindStep = -1;
                        });
    c.BindEventCallback("au_kind_up", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            m_pendingKindLine = intArg(args);
                            m_pendingKindStep = 1;
                        });
    c.BindEventCallback("au_find", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_pendingFind = true; });
    // Enter in the name field, as the field sees it: the text input keeps the key from the game's own key state.
    c.BindEventCallback("au_field_key",
                        [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
                        {
                            const auto key = static_cast<Rml::Input::KeyIdentifier>(event.GetParameter<int>("key_identifier", 0));
                            if (key != Rml::Input::KI_RETURN && key != Rml::Input::KI_NUMPADENTER)
                                return;
                            Rml::Element* field = event.GetTargetElement();
                            if (field != nullptr && field->GetId() == "au_search")
                                m_pendingFind = true;
                            else if (field != nullptr && field->GetId() == "au_bid")
                                m_pendingAction = ACT_BID;
                        });
    c.BindEventCallback("au_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_pendingExit = true; });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CAuctionWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    if (IsVisible())
    {
        if (m_tab == TAB_SEARCH)
            TakeFound();
        else if (m_tab == TAB_MINE)
            TakeMine();
        else
            TakeBag();
    }
    // The picked item's icon (#au_items over the panel); its camera fits the item into the icon's box.
    const bool showItem = IsVisible() && ShownItem() != nullptr;
    if (showItem)
        m_itemTarget.SetFieldOfView(ItemFieldOfView());
    m_itemTarget.Sync(GetField("au_items"), showItem);
    if (!m_RmlView.Document())
        return;

    // Layer depth UI::RmlBridge::ForegroundPanelLayerDepth: over the HUD and its logs.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    SyncContent();
}

void mu::ui::window::CAuctionWindow::SyncContent()
{
    AuctionRmlModel updated;
    updated.title = Line(I18N::Game::AuctionTitle, WINDOW_WIDTH, true);
    updated.tab = m_tab;
    updated.tabs = {
        Button(TAB_SEARCH, I18N::Game::AuctionTabSearch, THIRD_WIDTH, m_tab == TAB_SEARCH),
        Button(TAB_MINE, I18N::Game::AuctionTabMine, THIRD_WIDTH, m_tab == TAB_MINE),
        Button(TAB_SELL, I18N::Game::AuctionTabSell, THIRD_WIDTH, m_tab == TAB_SELL),
    };
    updated.itemName = Line({}, ITEM_LINES_WIDTH, true);
    updated.selFirst = Line({}, ITEM_LINES_WIDTH);
    updated.selSecond = Line({}, ITEM_LINES_WIDTH);
    updated.selThird = Line({}, CONTENT_WIDTH);
    updated.selFourth = Line({}, CONTENT_WIDTH);

    if (m_tab == TAB_SEARCH)
    {
        SyncSearch(updated);
        SyncSearchPick(updated);
    }
    else if (m_tab == TAB_MINE)
    {
        SyncMine(updated);
        SyncMinePick(updated);
    }
    else
    {
        SyncSell(updated);
        SyncSellPrice(updated);
    }

    updated.status = WrappedLine(m_status);
    updated.statusBad = m_statusBad;

    auto& binder = m_RmlView.Binder();
    SyncField(binder, &AuctionRmlModel::fieldPx, "field_px", UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal));
    SyncFieldFrom(binder, &AuctionRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::tab, "tab", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::tabs, "tabs", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::filters, "filters", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::toggles, "toggles", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::find, "find", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::listInfo, "list_info", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::rows, "rows", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::itemName, "item_name", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::selFirst, "sel_first", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::selSecond, "sel_second", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::selThird, "sel_third", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::selFourth, "sel_fourth", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::selAlert, "sel_alert", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::actions, "actions", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::moreActions, "more_actions", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::bidOpen, "bid_open", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::days, "days", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::priceLineCount, "price_line_count", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::price0, "price0", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::price1, "price1", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::price2, "price2", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::bidStep, "bid_step", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::fee, "fee", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &AuctionRmlModel::statusBad, "status_bad", updated);
    // The typed texts (the name searched, the price's counts, the bids) are the player's: the model owns them,
    // nothing writes them here (Clear sets the counts back, picking a lot fills its least next bid).
}

void mu::ui::window::CAuctionWindow::SyncSearch(AuctionRmlModel& updated)
{
    const auto& store = Auctions();
    const auto& found = store.Found();
    updated.filters = {
        Button(ACT_CATEGORY, CategoryName(m_category), HALF_WIDTH, m_category > 0),
        Button(ACT_CLASS, I18N::Game::AuctionMyClass, HALF_WIDTH, m_myClass),
    };
    updated.toggles = {
        Button(ACT_EXCELLENT, I18N::Game::AuctionExcellent, THIRD_WIDTH, m_excellent),
        Button(ACT_ANCIENT, I18N::Game::AuctionAncient, THIRD_WIDTH, m_ancient),
        Button(ACT_SORT, SortName(m_sort), THIRD_WIDTH, m_sort > 0),
    };
    updated.find = Button(0, I18N::Game::AuctionFind, QUARTER_WIDTH);

    const bool waiting = m_foundAsked == store.FoundVersion();
    if (waiting)
        updated.listInfo = WaitLine(m_requestedAt, CONTENT_WIDTH);
    else if (found.total == 0)
        updated.listInfo = Line(I18N::Game::AuctionNoLots, CONTENT_WIDTH);
    else
        updated.listInfo = Line(Printf(I18N::Game::AuctionPageInfo, found.total, found.page, found.pages), CONTENT_WIDTH);

    const int picked = FoundIndex();
    updated.rows = m_foundRows;
    for (auto& row : updated.rows)
        row.selected = row.index == picked;

    // < Buy [bid] Bid >: Buy for a lot with a buyout, Bid for one that takes bids and the player's bid doesn't lead.
    const auto* lot = picked >= 0 ? &found.lots[static_cast<size_t>(picked)] : nullptr;
    const bool canBuy = lot != nullptr && !lot->own && !lot->price.empty();
    const bool canBid = lot != nullptr && !lot->own && lot->TakesBids() && !lot->leading;
    updated.actions = {
        Button(ACT_PREVIOUS, L"<", ARROW_WIDTH, false, found.page <= 1),
        Button(ACT_BUY, I18N::Game::AuctionBuy, QUARTER_WIDTH, false, !canBuy),
    };
    updated.moreActions = {
        Button(ACT_BID, I18N::Game::AuctionBid, QUARTER_WIDTH, false, !canBid),
        Button(ACT_NEXT, L">", ARROW_WIDTH, false, found.page >= found.pages),
    };
    updated.bidOpen = canBid;

    // The bid field shows the least next bid of the lot picked, again when that changes.
    const int lotNumber = lot != nullptr ? lot->number : -1;
    const int next = canBid ? std::max(1, lot->nextBid) : -1;
    if (lotNumber != m_bidFilledLot || next != m_bidFilledNext)
    {
        m_bidFilledLot = lotNumber;
        m_bidFilledNext = next;
        SyncField(m_RmlView.Binder(), &AuctionRmlModel::bidText, "bid_text", next > 0 ? Rml::String(std::to_string(next)) : Rml::String());
    }
}

void mu::ui::window::CAuctionWindow::SyncSearchPick(AuctionRmlModel& updated)
{
    const int picked = FoundIndex();
    if (picked < 0)
    {
        const bool any = !Auctions().Found().lots.empty();
        updated.itemName = Line(any ? std::wstring(I18N::Game::AuctionPickLot) : std::wstring(), ITEM_LINES_WIDTH, true);
        return;
    }

    // The picked lot: its item (its tooltip while the icon is hovered), seller, CP, price and the bank's jewels.
    const auto& lot = Auctions().Found().lots[static_cast<size_t>(picked)];
    const ITEM* item = m_foundItems[static_cast<size_t>(picked)].get();
    updated.itemName = ClippedLine(ItemTitle(item), ITEM_LINES_WIDTH, true);
    updated.selFirst = ClippedLine(Printf(I18N::Game::AuctionSeller, lot.seller.c_str()), ITEM_LINES_WIDTH);
    if (lot.combatPower > 0)
        updated.selSecond = Line(Printf(I18N::Game::AuctionCp, Thousands(lot.combatPower).c_str()), ITEM_LINES_WIDTH);
    updated.selThird = lot.price.empty() ? ClippedLine(I18N::Game::AuctionBidsOnly, CONTENT_WIDTH)
                                         : ClippedLine(Printf(I18N::Game::AuctionPrice, PriceText(lot.price).c_str()), CONTENT_WIDTH);
    if (lot.own)
    {
        updated.selFourth = Line(I18N::Game::AuctionOwnLot, CONTENT_WIDTH);
    }
    else if (lot.TakesBids())
    {
        // The bids: the player's leads, the leading one and the least next, or none yet; red while the bank holds
        // fewer than the least next bid.
        const std::wstring kind = KindName(lot.bidKind);
        if (lot.leading)
            updated.selFourth = ClippedLine(Printf(I18N::Game::AuctionBidLeads, lot.bidCount, kind.c_str()), CONTENT_WIDTH);
        else if (lot.bidCount > 0)
            updated.selFourth = ClippedLine(Printf(I18N::Game::AuctionBidInfo, lot.bidCount, kind.c_str(), lot.bids, lot.nextBid), CONTENT_WIDTH);
        else
            updated.selFourth = ClippedLine(Printf(I18N::Game::AuctionBidStart, lot.nextBid, kind.c_str()), CONTENT_WIDTH);
        updated.selAlert = !lot.leading && Bank().HasInfo() && Bank().Count(lot.bidKind) < lot.nextBid;
    }
    else if (Bank().HasInfo())
    {
        bool missing = false;
        const std::wstring held = BankText(lot.price, missing);
        updated.selFourth = ClippedLine(Printf(I18N::Game::AuctionYourBank, held.c_str()), CONTENT_WIDTH);
        updated.selAlert = missing;
    }
}

void mu::ui::window::CAuctionWindow::SyncMine(AuctionRmlModel& updated)
{
    using GameLogic::Auction::OwnLotState;
    const auto& store = Auctions();
    const auto& mine = store.Mine();
    const bool waiting = m_mineAsked == store.MineVersion();
    if (waiting)
        updated.listInfo = WaitLine(m_requestedAt, CONTENT_WIDTH);
    else if (mine.lots.empty())
        updated.listInfo = Line(I18N::Game::AuctionMineNone, CONTENT_WIDTH);
    else
        updated.listInfo = Line(Printf(I18N::Game::AuctionMineInfo, mine.count, mine.onSale, mine.maximumLots), CONTENT_WIDTH);

    const int picked = MineIndex();
    updated.rows = m_mineRows;
    for (auto& row : updated.rows)
        row.selected = row.index == picked;

    // Remove for an own lot on sale without a bid; Take for one where jewels or an item wait.
    const auto* lot = picked >= 0 ? &mine.lots[static_cast<size_t>(picked)] : nullptr;
    const bool removable = lot != nullptr && !lot->bidder && lot->state == OwnLotState::Active && lot->bidCount == 0;
    const bool any = std::any_of(mine.lots.begin(), mine.lots.end(), [](const auto& each) { return each.Takeable(); });
    updated.actions = {
        Button(ACT_CANCEL, I18N::Game::AuctionCancel, QUARTER_WIDTH, false, !removable),
        Button(ACT_TAKE, I18N::Game::AuctionTake, QUARTER_WIDTH, false, lot == nullptr || !lot->Takeable()),
        Button(ACT_TAKE_ALL, I18N::Game::AuctionTakeAll, QUARTER_WIDTH, false, !any),
        Button(ACT_REFRESH, I18N::Game::AuctionRefresh, QUARTER_WIDTH),
    };
}

void mu::ui::window::CAuctionWindow::SyncMinePick(AuctionRmlModel& updated)
{
    using GameLogic::Auction::OwnLotState;
    const int picked = MineIndex();
    if (picked < 0)
        return;

    // The picked lot: its item, state, CP, price and what Take does with it.
    const auto& mine = Auctions().Mine();
    const auto& lot = mine.lots[static_cast<size_t>(picked)];
    const ITEM* item = m_mineItems[static_cast<size_t>(picked)].get();
    updated.itemName = ClippedLine(ItemTitle(item), ITEM_LINES_WIDTH, true);
    updated.selFirst = ClippedLine(StateText(lot, mine.receivedAt), ITEM_LINES_WIDTH);
    if (const int cp = CombatPower(item); cp > 0)
        updated.selSecond = Line(Printf(I18N::Game::AuctionCp, Thousands(cp).c_str()), ITEM_LINES_WIDTH);
    if (lot.bidder)
    {
        // A lot the player bid on: its seller, then the player's bid or what Take does.
        updated.selThird = ClippedLine(Printf(I18N::Game::AuctionSeller, lot.other.c_str()), CONTENT_WIDTH);
        if (lot.state == OwnLotState::Leading)
            updated.selFourth = ClippedLine(Printf(I18N::Game::AuctionYourBid, lot.mine, KindName(lot.bidKind).c_str()), CONTENT_WIDTH);
        else if (lot.state == OwnLotState::Outbid)
            updated.selFourth = ClippedLine(I18N::Game::AuctionTakeBid, CONTENT_WIDTH);
        else if (lot.state == OwnLotState::Won)
            updated.selFourth = ClippedLine(I18N::Game::AuctionTakeItem, CONTENT_WIDTH);
        return;
    }

    updated.selThird = lot.price.empty() ? ClippedLine(I18N::Game::AuctionBidsOnly, CONTENT_WIDTH)
                                         : ClippedLine(Printf(I18N::Game::AuctionPrice, PriceText(lot.price).c_str()), CONTENT_WIDTH);
    if (lot.state == OwnLotState::Sold)
        updated.selFourth = ClippedLine(I18N::Game::AuctionTakeJewels, CONTENT_WIDTH);
    else if (lot.state == OwnLotState::Unsold)
        updated.selFourth = ClippedLine(I18N::Game::AuctionTakeItem, CONTENT_WIDTH);
    else if (lot.state == OwnLotState::Active && lot.bidCount > 0)
        updated.selFourth = ClippedLine(I18N::Game::AuctionHasBid, CONTENT_WIDTH);
}

void mu::ui::window::CAuctionWindow::SyncSell(AuctionRmlModel& updated)
{
    updated.listInfo = Line(m_bag.empty() ? I18N::Game::AuctionSellEmpty : I18N::Game::AuctionSellInfo, CONTENT_WIDTH);

    const int picked = BagIndex();
    updated.rows = m_bagRows;
    for (auto& row : updated.rows)
        row.selected = row.index == picked;

    // The picked item: its name and CP beside its icon.
    if (picked >= 0)
    {
        const ITEM* item = m_bag[static_cast<size_t>(picked)].item.get();
        updated.itemName = ClippedLine(ItemTitle(item), ITEM_LINES_WIDTH, true);
        if (const int cp = CombatPower(item); cp > 0)
            updated.selFirst = Line(Printf(I18N::Game::AuctionCp, Thousands(cp).c_str()), ITEM_LINES_WIDTH);
    }
    else if (!m_bag.empty())
    {
        updated.itemName = Line(I18N::Game::AuctionPickItem, ITEM_LINES_WIDTH, true);
    }

    const bool ready = Auctions().HasSettings();
    updated.actions = {
        Button(ACT_SELL, I18N::Game::AuctionSell, HALF_WIDTH, false, picked < 0 || !ready),
        Button(ACT_CLEAR, I18N::Game::AuctionClear, HALF_WIDTH),
    };
}

void mu::ui::window::CAuctionWindow::SyncSellPrice(AuctionRmlModel& updated)
{
    const auto& store = Auctions();
    const auto& settings = store.GetSettings();
    if (!store.HasSettings())
    {
        updated.priceLineCount = 0;
        updated.fee = WaitLine(m_requestedAt, CONTENT_WIDTH);
        return;
    }

    // The days on sale (the settings' durations; the middle one at first).
    const int durations = static_cast<int>(settings.durations.size());
    if (durations > 0)
        m_days = std::clamp(m_days, 0, durations - 1);
    for (int i = 0; i < durations; ++i)
        updated.days.push_back(Button(i, Printf(I18N::Game::AuctionDays, settings.durations[static_cast<size_t>(i)]), THIRD_WIDTH, i == m_days));

    // The buyout's lines: the first asks for Jewels of Bless (or the first jewel the server takes) at first; every
    // line may be set to none, and with none at all the lot goes for bids only.
    updated.priceLineCount = std::clamp(settings.priceLines, 1, PriceLineSlots);
    if (m_priceKinds[0] == UnsetKind || (m_priceKinds[0] != NoKind && !settings.Allows(m_priceKinds[0])))
    {
        m_priceKinds[0] = NoKind;
        if (settings.Allows(FirstKind))
            m_priceKinds[0] = FirstKind;
        else
            StepKind(0, 1);
    }
    for (int i = 1; i < PriceLineSlots; ++i)
    {
        int& kind = m_priceKinds[static_cast<size_t>(i)];
        if (i >= updated.priceLineCount || (kind != NoKind && !settings.Allows(kind)))
            kind = NoKind;
    }
    const bool noBuyout = std::all_of(m_priceKinds.begin(), m_priceKinds.end(), [](int kind) { return kind < 0; });
    AuctionPriceStep* steps[PriceLineSlots] = {&updated.price0, &updated.price1, &updated.price2};
    for (int i = 0; i < PriceLineSlots; ++i)
    {
        const int kind = m_priceKinds[static_cast<size_t>(i)];
        steps[i]->kind = Line(i == 0 && noBuyout ? std::wstring(I18N::Game::AuctionNoBuyout) : KindName(kind), KIND_WIDTH);
        steps[i]->used = kind >= 0;
    }

    // The starting bid: none at first; with a jewel the lot takes bids in it from the count typed.
    if (m_bidKind != NoKind && !settings.Allows(m_bidKind))
        m_bidKind = NoKind;
    updated.bidStep.kind = Line(m_bidKind < 0 ? std::wstring(I18N::Game::AuctionNoBids) : Printf(I18N::Game::AuctionBidKind, KindName(m_bidKind).c_str()),
                                KIND_WIDTH);
    updated.bidStep.used = m_bidKind >= 0;

    const int days = durations > 0 ? settings.durations[static_cast<size_t>(m_days)] : 0;
    updated.fee = ClippedLine(Printf(I18N::Game::AuctionFee, Thousands(settings.feePerDay * days).c_str(), settings.onSale, settings.maximumLots),
                              CONTENT_WIDTH);
}
