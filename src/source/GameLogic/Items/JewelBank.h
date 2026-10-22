#pragma once

#include <array>
#include <chrono>
#include <string>

// The jewel bank, from our server's add-on (MU Moldova, JewelBank): an account keeps counts of ten kinds of
// jewels instead of the jewels themselves. The jewel bank window (a button in the inventory) shows them.
//
// The window asks "/jbinfo"; the server answers with a blue message this takes out of the chat: "#JBK|count|…",
// a count per kind in Kinds() order, or "#JBK!|" when the bank isn't set up. "/jbdep kind" puts every jewel of a
// kind from the inventory into the bank (packed ones too) and "/jbwd kind amount" takes jewels out (kind from 1);
// both answer with the new counts and a message in the chat.
namespace GameLogic::JewelBank
{
struct Kind
{
    // The single jewel's item type (group * MAX_ITEM_INDEX + number), the one the bank gives out.
    int itemType = 0;
    // The packed jewels' item type (group 12): level 0, 1 and 2 hold 10, 20 and 30.
    int packedType = 0;
};

constexpr int KindCount = 10;

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

    // Takes up to `amount` jewels of a kind out of the bank.
    void Withdraw(int kind, int amount);

    bool HasInfo() const { return m_hasInfo; }
    // The server answered that the bank isn't set up.
    bool IsUnavailable() const { return m_unavailable; }
    int Count(int kind) const { return kind >= 0 && kind < KindCount ? m_counts[kind] : 0; }
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
