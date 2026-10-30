#pragma once

#include <chrono>
#include <string>
#include <vector>

// Lugard's shop for tower points, the Endless Tower window's Shop tab (stage V3).
//
// Each member of a tower run that gives rewards earns its account tower points (the height plus a bonus per boss),
// spent at Lugard in Lorencia or Devias with limits per UTC calendar month. "/towershop" asks for the shop, which
// comes as blue server messages that this takes out of the chat:
//   "#ETS0|balance|month yyyy-MM|seconds to the next month|near Lugard|waiting items|points a wave|points a boss"
//   "#ETS|key|name|group|number|level|count|price|monthly limit (0 none)|bought this month|into the jewel bank"
//   "#ETS.|"
// "/towerbuy key count" buys; its answer is "#ETK|ok|text", followed by the shop's lines again when it went through.
namespace GameLogic::EndlessTower
{
struct ShopItem
{
    // What the client buys it by.
    std::wstring Key;
    std::wstring Name;
    int Group = 0;
    int Number = 0;
    int Level = 0;
    // Pieces one purchase gives.
    int Count = 1;
    int Price = 0;
    // Purchases an account may make a month (0 = no limit), and those made this month.
    int Limit = 0;
    int Bought = 0;
    // A jewel of the jewel bank goes there, anything else into the bag.
    bool Bank = false;
};

struct ShopStatus
{
    long long Balance = 0;
    // The UTC month the limits count in ("2026-10"), and the seconds to the next one when the answer came.
    std::wstring Month;
    int SecondsToMonth = 0;
    // This character stands at Lugard, where it can buy.
    bool Near = false;
    // Items bought or won that wait for room in the bag.
    long long Waiting = 0;
    int PointsPerWave = 1;
    int PointsPerBoss = 10;
    std::chrono::steady_clock::time_point ReceivedAt{};
};

class Shop
{
public:
    // A purchase takes 1 to this many at once (the server's limit).
    static constexpr int MaximumBuyCount = 100;

    static Shop& Instance();

    // Takes a server message. True when it was one of the shop's lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the shop; at most once every few seconds.
    void Request();

    // Buys count pieces of the item with the key; nothing while a purchase is awaited or for a count out of range.
    void Buy(const std::wstring& key, int count);

    // How many of the item this account can buy now: the points, the month's limit and the server's maximum.
    int MostBuyable(const ShopItem& item) const;

    bool HasShop() const { return m_hasShop; }

    const ShopStatus& GetStatus() const { return m_status; }
    const std::vector<ShopItem>& Items() const { return m_items; }

    // The answer to the last purchase, and whether one is still awaited.
    bool IsBuying() const { return m_buying; }
    bool LastBuySucceeded() const { return m_lastBuyOk; }
    const std::wstring& LastBuyText() const { return m_lastBuyText; }
    // Counts up on every answer, so the window notices a new one.
    int BuyAnswers() const { return m_buyAnswers; }

    // Changes whenever new data came, so the window rebuilds only then.
    int Version() const { return m_version; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    bool TakeHeader(const std::vector<std::wstring>& fields);
    void TakeItem(const std::vector<std::wstring>& fields);
    void TakeAnswer(const std::vector<std::wstring>& fields);
    void ExpireBuy();

    ShopStatus m_status;
    std::vector<ShopItem> m_items;
    ShopStatus m_incomingStatus;
    std::vector<ShopItem> m_incomingItems;
    bool m_receiving = false;
    bool m_hasShop = false;
    bool m_buying = false;
    bool m_lastBuyOk = false;
    std::wstring m_lastBuyText;
    int m_buyAnswers = 0;
    int m_version = 0;
    std::chrono::steady_clock::time_point m_lastRequest{};
    std::chrono::steady_clock::time_point m_buySentAt{};
};

inline Shop& TowerShop()
{
    return Shop::Instance();
}
} // namespace GameLogic::EndlessTower
