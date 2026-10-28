#pragma once

#include "GameLogic/Items/Auction.h"
#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Inventory/AuctionRmlModel.h"
#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

typedef struct tagITEM ITEM;

namespace Rml
{
class Element;
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The auction window (GameLogic/Items/Auction.h), opened by talking to Raul the Auctioneer and docked beside the
// inventory, which opens with it. Three tabs:
//   Search: the lots on sale by category, for the hero's class, excellent or ancient only, a name, in an order; a
//     page at a time. A row is the item and its price (buyout, bid) and time left (the item's tooltip while hovered);
//     the picked lot shows its item, seller, price, bids and what the jewel bank holds of it; Buy pays the buyout,
//     Bid bids the count typed beside it (the least next bid at first); both ask first.
//   My lots: the account's lots on sale, sold or not sold, and the lots it bid on; Remove takes one without a bid
//     off sale, Take brings the jewels of a sold lot or a beaten bid into the jewel bank, or an unsold or won item
//     into the bag, Take all does every one.
//   Sell: the bag's items; the picked one goes on sale for some days, for a buyout of up to three lines of jewels,
//     for bids in one jewel from a starting count, or both, for a fee in zen a day (asked first).
// It closes with the inventory and when the hero walks away from the auctioneer. auction.rml draws it; C++ draws the
// picked item into its icon box through #au_items.
class CAuctionWindow : public CObject
{
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px), as auction.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        // A row's lines: the content less the scrollbar's lane and the row's padding.
        ROW_WIDTH = 140,
        // The picked item's lines, beside its icon.
        ITEM_LINES_WIDTH = 120,
        THIRD_WIDTH = 54,
        QUARTER_WIDTH = 40,
        HALF_WIDTH = 82,
        // A price line's jewel, between its arrows.
        KIND_WIDTH = 70,
        // The page arrows beside Buy and Bid.
        ARROW_WIDTH = 18,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CAuctionWindow();
    ~CAuctionWindow() override;

    bool Create(CManager* pNewUIMng);
    void Release();

    bool UpdateMouseEvent() override;
    Rml::ElementDocument* GetPlacedDocument() const override { return m_RmlView.Document(); }
    bool UpdateKeyEvent() override;
    // Escape and Enter while a field of the window is typed in.
    bool TakesTypingFrom(const Rml::ElementDocument* document) const override
    {
        return document != nullptr && document == m_RmlView.Document();
    }
    bool Update() override;
    bool Render() override;

    float GetLayerDepth() override;
    float GetKeyEventOrder() override;

    void OpenningProcess();
    void ClosingProcess();

private:
    enum Tab
    {
        TAB_SEARCH,
        TAB_MINE,
        TAB_SELL,
    };
    enum Action
    {
        ACT_CATEGORY = 1,
        ACT_CLASS,
        ACT_EXCELLENT,
        ACT_ANCIENT,
        ACT_SORT,
        ACT_PREVIOUS,
        ACT_NEXT,
        ACT_BUY,
        ACT_CANCEL,
        ACT_TAKE,
        ACT_TAKE_ALL,
        ACT_REFRESH,
        ACT_SELL,
        ACT_CLEAR,
        ACT_BID,
    };
    static constexpr int PriceLineSlots = 3;
    // The Sell tab's line of the starting bid, after the price's lines (au_kind_down/up's line).
    static constexpr int BidLine = PriceLineSlots;
    // A price line without a jewel.
    static constexpr int NoKind = -1;
    // The price's first line before the Sell tab showed it: it then asks for Jewels of Bless.
    static constexpr int UnsetKind = -2;

    // An item of the bag the Sell tab lists: a copy (the bag may change under it) and its slot.
    struct BagItem
    {
        int slot = 0;
        std::unique_ptr<ITEM> item;
    };
    // What a bag's item was made from: its slot, type, level, durability, excellent and ancient bytes.
    using BagKey = std::array<int, 6>;

    void BindRmlModel(Rml::DataModelConstructor& c, AuctionRmlModel& model);
    void SyncRmlModel();
    void SyncContent();
    void SyncSearch(AuctionRmlModel& updated);
    void SyncSearchPick(AuctionRmlModel& updated);
    void SyncMine(AuctionRmlModel& updated);
    void SyncMinePick(AuctionRmlModel& updated);
    void SyncSell(AuctionRmlModel& updated);
    void SyncSellPrice(AuctionRmlModel& updated);

    // Closes the window when the inventory closed or the hero is away from the auctioneer; true when it did.
    bool CloseWhenAway();
    void HandleClicks();
    void SwitchTab(int tab);
    void PickRow(int row);
    void DoAction(int action);
    void StepKind(int line, int step);
    void TakeResult();

    // Asks for the current tab's list (the search's page, the own lots, the settings).
    void RequestCurrent();
    void Search(int page);
    void RequestMine();
    void Buy();
    void Bid();
    void CancelPicked();
    void Take(bool all);
    void Sell();
    void ClearSell();
    // The buyout typed on the Sell tab (empty: none) and the starting bid (kind NoKind: none); false, and a status
    // line saying why, when it's wrong.
    bool TypedPrice(GameLogic::Auction::Price& price);
    bool TypedBid(int& kind, int& start);

    // The lists' items and rows, made again when their answer changed (the bag's: when the bag did) and now and
    // then for the time left.
    void TakeFound();
    void TakeMine();
    void TakeBag();
    int FoundIndex() const;
    int MineIndex() const;
    int BagIndex() const;
    // The item a list's row shows, or null.
    ITEM* RowItem(int index) const;
    // The picked one's item, which the icon shows.
    ITEM* ShownItem() const;
    float ItemFieldOfView() const;
    void RenderItems();
    void SetStatus(const std::wstring& text, bool bad);
    Rml::Element* GetField(const char* id) const;

    CManager* m_pNewUIMng = nullptr;
    int m_tab = TAB_SEARCH;
    std::chrono::steady_clock::time_point m_requestedAt{};
    int m_seenResult = 0;
    int m_seenSold = 0;
    std::wstring m_status;
    bool m_statusBad = false;

    // Search: the filters, the page asked for, the lot picked (its number), the page's items and rows, and the
    // list's version when it was asked for (no answer since: still waiting).
    int m_category = 0;
    bool m_myClass = false;
    bool m_excellent = false;
    bool m_ancient = false;
    int m_sort = 0;
    int m_page = 1;
    int m_searchPicked = -1;
    int m_foundSeen = -1;
    int m_foundAsked = -1;
    std::vector<std::unique_ptr<ITEM>> m_foundItems;
    std::vector<AuctionRow> m_foundRows;
    // The lot and least next bid the bid field was filled for: another one fills it again.
    int m_bidFilledLot = -1;
    int m_bidFilledNext = -1;

    // My lots: the picked one (its number), the items and rows.
    int m_minePicked = -1;
    int m_mineSeen = -1;
    int m_mineAsked = -1;
    std::vector<std::unique_ptr<ITEM>> m_mineItems;
    std::vector<AuctionRow> m_mineRows;

    // Sell: the bag's items and rows (and what they were made from), the picked one's slot, the days (an index into
    // the settings' durations), the price's jewels and the starting bid's (kinds, or NoKind).
    std::vector<BagItem> m_bag;
    std::vector<AuctionRow> m_bagRows;
    std::vector<BagKey> m_bagKeys;
    int m_sellSlot = -1;
    int m_days = 1;
    std::array<int, PriceLineSlots> m_priceKinds{UnsetKind, NoKind, NoKind};
    int m_bidKind = NoKind;

    // The rows' second lines count the time down: made again this often.
    std::chrono::steady_clock::time_point m_rowsBuiltAt{};

    // Clicks RmlUi reported, handled in Update().
    int m_pendingTab = -1;
    int m_pendingAction = 0;
    int m_pendingRow = -1;
    int m_pendingDays = -1;
    int m_pendingKindLine = -1;
    int m_pendingKindStep = 0;
    bool m_pendingFind = false;
    bool m_pendingExit = false;

    UI::Items::ItemCameraTarget m_itemTarget{[this](const Rml::Vector2f&, const Rml::Vector2f&) { RenderItems(); }};

    UI::RmlBridge::ThemedView<AuctionRmlModel> m_RmlView{"auction",
        [this](Rml::DataModelConstructor& c, AuctionRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/auction.rml"}}};
};
} // namespace mu::ui::window
