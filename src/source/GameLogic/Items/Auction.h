#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

// The auction at Raul the Auctioneer (NPC 546), from our server's add-on (MU Moldova, Auction): a player puts an item
// from the bag on sale for a buyout price of up to three lines of jewels, for bids in one jewel kind, or both; the
// buyer pays from the jewel bank (GameLogic/Items/JewelBank.h), a bidder's jewels wait in the bid until it is beaten
// (they come back) or wins, and the seller takes them into the bank, or the item back when nobody bought it. Items go
// for jewels only, never for zen; the seller pays a fee in zen for each day on sale.
//
// The auction window (UI/Inventory/AuctionWindow.h) sends "/auc ..." chat lines; the answers are blue messages this
// takes out of the chat (the add-on's Auction.cs lists them):
//   "#AUO|fee a day|days,...|lots per account|lots on sale|price lines|most of one jewel|kinds|page size|bid step %|
//     least bid step|minutes a late bid adds": the settings. Talking to Raul sends them (and the jewel bank's
//     counts): the window opens on them.
//   "#AUL0|total|page|pages", then "#AUL|lot|version|item|price|seconds left|seller|CP|own|bid|bids|next bid|lead"
//     per lot, then "#AUL.|": a page of the lots on sale ("/auc find ..."). The price is the buyout (empty: bids
//     only); the bid "kind:leading count" (0 before the first bid; empty: no bids); lead 1 = the player's bid leads.
//   "#AUM0|lots|on sale|lots per account", then "#AUM|lot|state|item|price|seconds left|other|role|bid|bids|mine" per
//     lot, then "#AUM.|": the player's own lots (role s: active, sold, unsold or busy; other the buyer or the leading
//     bidder) and the lots it bid on (role b: leading, outbid, won or busy; other the seller; mine its bid's jewels)
//     ("/auc mine").
//   "#AUR|ok|op|text" or "#AUR|err|op|text": the answer to "/auc sell", "buy", "bid", "cancel" or "take".
//   "#AUN|sold|lot|item|buyer": a lot of the player's sold or went to a bid; "#AUN|won|lot|item|seller": the
//     player's bid won; "#AUN|outbid|lot|item|count": its bid was beaten, and this answers "/auc refund" (the
//     jewels back into the bank).
// An item is its bytes in hex (the server's item serializer); a price is "kind:count,..." with the jewel bank's kinds
// counted from 1 (here they are counted from 0, as GameLogic::JewelBank::Kinds() is).
namespace GameLogic::Auction
{
// A line of a price: a jewel bank kind (an index into GameLogic::JewelBank::Kinds()) and how many of it.
struct PriceLine
{
    int kind = 0;
    int count = 0;

    bool operator==(const PriceLine&) const = default;
};
using Price = std::vector<PriceLine>;

// What the server allows ("#AUO").
struct Settings
{
    long long feePerDay = 0;
    std::vector<int> durations;
    int maximumLots = 0;
    int onSale = 0;
    int priceLines = 0;
    int maximumCount = 0;
    // Bit k: the jewel bank's kind k may be asked for.
    unsigned kinds = 0;
    int pageSize = 0;
    // A bid beats the leading one by this percent of it, at least by bidMinimumStep jewels; one in the last
    // bidExtendMinutes minutes keeps the lot on sale that long after it.
    int bidStepPercent = 5;
    int bidMinimumStep = 1;
    int bidExtendMinutes = 5;

    bool Allows(int kind) const;
};

// A lot on sale ("#AUL"); its version changes with each change of the lot, and a purchase names it.
struct Lot
{
    int number = 0;
    int version = 0;
    std::vector<std::uint8_t> item;
    Price price;
    long long secondsLeft = 0;
    std::wstring seller;
    long long combatPower = 0;
    // A lot of the player's own account: it can't be bought.
    bool own = false;
    // The bids' jewel kind (-1: the lot takes none), the leading bid (0: none yet), how many were placed, the least
    // next bid, and whether the player's bid leads.
    int bidKind = -1;
    int bidCount = 0;
    int bids = 0;
    int nextBid = 0;
    bool leading = false;

    bool TakesBids() const { return bidKind >= 0; }
};

// A page of the lots on sale; their seconds count from receivedAt.
struct LotPage
{
    int total = 0;
    int page = 1;
    int pages = 1;
    std::vector<Lot> lots;
    std::chrono::steady_clock::time_point receivedAt{};
};

enum class OwnLotState
{
    // On sale.
    Active,
    // Bought or won by a bid: the jewels wait to be taken.
    Sold,
    // Not bought in its time, or taken off sale: the item waits to be taken.
    Unsold,
    // The server is in the middle of something with it.
    Busy,
    // A lot the player bid on: its bid leads.
    Leading,
    // Its bid was beaten: the jewels wait to be taken back.
    Outbid,
    // Its bid won: the item waits to be taken.
    Won,
};

// A lot of the player's own, or one it bid on (bidder).
struct OwnLot
{
    int number = 0;
    OwnLotState state = OwnLotState::Active;
    std::vector<std::uint8_t> item;
    // The buyout (empty: bids only); a lot sold by a bid: that bid.
    Price price;
    long long secondsLeft = 0;
    // The buyer or the leading bidder; on a bidder's row the seller.
    std::wstring other;
    bool bidder = false;
    // As Lot's; mine is the player's bid on a bidder's row (its leading bid, the outbid jewels or the winning bid).
    int bidKind = -1;
    int bidCount = 0;
    int bids = 0;
    int mine = 0;

    // Take does something with it: jewels or an item wait.
    bool Takeable() const
    {
        return state == OwnLotState::Sold || state == OwnLotState::Unsold || state == OwnLotState::Outbid || state == OwnLotState::Won;
    }
};

// The player's own lots ("#AUM"); their seconds count from receivedAt.
struct OwnLots
{
    int count = 0;
    int onSale = 0;
    int maximumLots = 0;
    std::vector<OwnLot> lots;
    std::chrono::steady_clock::time_point receivedAt{};
};

// The answer to the last sell, buy, bid, cancel or take ("#AUR").
struct Result
{
    bool ok = false;
    std::wstring op;
    std::wstring text;
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was an auction line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);
    // True once after Raul sent the settings: the window opens.
    bool TakeOpenRequest();

    // Sends "/auc " + request.
    void Send(const std::wstring& request);
    // Asks for the settings again ("/auc info"); their answer opens nothing.
    void RequestSettings();

    bool HasSettings() const { return m_hasSettings; }
    const Settings& GetSettings() const { return m_settings; }
    const LotPage& Found() const { return m_found; }
    int FoundVersion() const { return m_foundVersion; }
    const OwnLots& Mine() const { return m_mine; }
    int MineVersion() const { return m_mineVersion; }
    const Result& LastResult() const { return m_result; }
    int ResultVersion() const { return m_resultVersion; }
    // Changes when a lot of the player's sold, or a bid of its won or was beaten.
    int SoldVersion() const { return m_soldVersion; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    void TakeSettings(const std::vector<std::wstring>& fields);
    void TakePageHeader(const std::vector<std::wstring>& fields);
    void TakeLot(const std::vector<std::wstring>& fields);
    // The page's or the own lots' end line: what came since the header is the new list.
    void FinishPage();
    void TakeOwnHeader(const std::vector<std::wstring>& fields);
    void TakeOwnLot(const std::vector<std::wstring>& fields);
    void FinishOwnLots();
    void TakeResult(const std::vector<std::wstring>& fields);
    void TakeNotice(const std::vector<std::wstring>& fields);

    Settings m_settings;
    bool m_hasSettings = false;
    bool m_openRequested = false;
    // The settings were asked for by RequestSettings(): their answer doesn't open the window.
    bool m_settingsAsked = false;

    LotPage m_found;
    LotPage m_foundIncoming;
    bool m_receivingFound = false;
    int m_foundVersion = 0;

    OwnLots m_mine;
    OwnLots m_mineIncoming;
    bool m_receivingMine = false;
    int m_mineVersion = 0;

    Result m_result;
    int m_resultVersion = 0;
    int m_soldVersion = 0;
};

inline Store& Auctions()
{
    return Store::Instance();
}
} // namespace GameLogic::Auction
