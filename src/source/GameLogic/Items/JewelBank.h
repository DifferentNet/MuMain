#pragma once

#include <array>
#include <chrono>
#include <string>

// The jewel bank, from our server's add-on (MU Moldova, JewelBank): an account keeps counts of twelve kinds of
// jewels and of Rena instead of the items themselves. The jewel bank window (a button in the inventory) shows them.
//
// The window asks "/jbinfo"; the server answers with a blue message this takes out of the chat: "#JBK|count|…",
// a count per kind in Kinds() order, or "#JBK!|" when the bank isn't set up. The last three kinds (Jewel of Ancient,
// Jewel of Fate, Rena) are optional: -1 when the server hasn't set them up, and an older server sends only the first
// ten or twelve.
// "/jbdep kind" puts every jewel of a kind from the inventory into the bank (packed ones too) and "/jbwd kind
// amount" takes jewels out (kind from 1); both answer with the new counts and a message in the chat. Rena (kind 13,
// 2026-11-23) goes in as stacks of level 0 Rena (a stack counts its size) and comes out in stacks of up to 255. The
// auction (GameLogic/Items/Auction.h) prices its lots in the first PriceKindCount kinds and pays from the bank.
namespace GameLogic::JewelBank
{
struct Kind
{
    // The single jewel's item type (group * MAX_ITEM_INDEX + number), the one the bank gives out.
    int itemType = 0;
    // The packed jewels' item type (group 12): level 0, 1 and 2 hold 10, 20 and 30; -1 for a kind without packs.
    int packedType = 0;
    // Kept as stacks of level 0 items (Rena, whose item type the Stones and the Mark of Lord share at levels 1-3):
    // a stack counts its durability.
    bool stacked = false;
};

// The ten kinds every bank has, then the Jewel of Ancient (14:160), the Jewel of Fate (14:161) and Rena (14:21).
constexpr int BaseKindCount = 10;
constexpr int KindCount = 13;
// The kinds a price can name (the auction): all but Rena, which is the bank's alone on the server too.
constexpr int PriceKindCount = 12;

// The kinds in the server's order (JewelBank.Kinds in server/addons/src/JewelBank.cs).
const std::array<Kind, KindCount>& Kinds();

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was a jewel bank line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the counts; at most once a second.
    void Request();

    // Puts every jewel of a kind (index into Kinds()) from the inventory into the bank.
    void Deposit(int kind);

    // Takes up to `amount` jewels of a kind out of the bank; with `packs` each 10 as one packed jewel, the rest
    // single (the server's /jbwdp).
    void Withdraw(int kind, int amount, bool packs = false);

    // Whether a kind has a packed form (the first ten; the Jewel of Ancient and the Jewel of Fate have none).
    static bool HasPacks(int kind) { return kind >= 0 && kind < BaseKindCount; }

    bool HasInfo() const { return m_hasInfo; }
    // The server answered that the bank isn't set up.
    bool IsUnavailable() const { return m_unavailable; }
    int Count(int kind) const { return kind >= 0 && kind < KindCount && m_counts[kind] > 0 ? m_counts[kind] : 0; }
    // The server keeps this kind (an optional one may be missing); false until the first answer.
    bool IsAvailable(int kind) const { return m_hasInfo && kind >= 0 && kind < KindCount && m_counts[kind] >= 0; }
    std::chrono::steady_clock::time_point LastRequest() const { return m_lastRequest; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    std::array<int, KindCount> m_counts{};
    bool m_hasInfo = false;
    bool m_unavailable = false;
    std::chrono::steady_clock::time_point m_lastRequest{};
};

inline Store& Bank()
{
    return Store::Instance();
}
} // namespace GameLogic::JewelBank
