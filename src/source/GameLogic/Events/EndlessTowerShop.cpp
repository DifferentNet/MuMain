#include "stdafx.h"
#include "GameLogic/Events/EndlessTowerShop.h"

#include "GameLogic/Events/EndlessTowerLines.h"

#include <algorithm>

namespace GameLogic::EndlessTower
{
namespace
{
// "#ETS0|" also starts with "#ETS", so the header is checked before the item line.
constexpr std::wstring_view Header = L"#ETS0|";
constexpr std::wstring_view ItemLine = L"#ETS|";
constexpr std::wstring_view Footer = L"#ETS.|";
constexpr std::wstring_view BuyAnswer = L"#ETK|";
constexpr std::wstring_view ShopCommand = L"/towershop";
constexpr std::wstring_view BuyCommand = L"/towerbuy ";
// Fields the lines need; the header's last two came later and are optional.
constexpr size_t HeaderFields = 5;
constexpr size_t FullHeaderFields = 7;
constexpr size_t ItemFields = 10;
// Repeated requests within this time are dropped, so switching tabs doesn't flood the server.
constexpr auto RequestInterval = std::chrono::seconds(2);
// A purchase the server didn't answer counts as answered after this time, so the buttons come back.
constexpr auto BuyTimeout = std::chrono::seconds(8);
} // namespace

Shop& Shop::Instance()
{
    static Shop shop;
    return shop;
}

bool Shop::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(Header))
    {
        m_receiving = TakeHeader(Split(view.substr(Header.size())));
        return true;
    }

    if (view.starts_with(ItemLine))
    {
        if (m_receiving)
            TakeItem(Split(view.substr(ItemLine.size())));
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_status = std::move(m_incomingStatus);
            m_items = std::move(m_incomingItems);
            m_status.ReceivedAt = std::chrono::steady_clock::now();
            m_hasShop = true;
            m_receiving = false;
            ++m_version;
        }
        return true;
    }

    if (view.starts_with(BuyAnswer))
    {
        TakeAnswer(Split(view.substr(BuyAnswer.size())));
        return true;
    }

    return false;
}

bool Shop::TakeHeader(const std::vector<std::wstring>& fields)
{
    m_incomingStatus = {};
    m_incomingItems.clear();
    if (fields.size() < HeaderFields)
        return false;

    m_incomingStatus.Balance = ToLong(fields[0]);
    m_incomingStatus.Month = fields[1];
    m_incomingStatus.SecondsToMonth = ToInt(fields[2]);
    m_incomingStatus.Near = fields[3] == L"1";
    m_incomingStatus.Waiting = ToLong(fields[4]);
    if (fields.size() >= FullHeaderFields)
    {
        m_incomingStatus.PointsPerWave = ToInt(fields[5]);
        m_incomingStatus.PointsPerBoss = ToInt(fields[6]);
    }
    return true;
}

void Shop::TakeItem(const std::vector<std::wstring>& fields)
{
    if (fields.size() < ItemFields || fields[0].empty())
        return;

    ShopItem item;
    item.Key = fields[0];
    item.Name = fields[1];
    item.Group = ToInt(fields[2]);
    item.Number = ToInt(fields[3]);
    item.Level = ToInt(fields[4]);
    item.Count = (std::max)(1, ToInt(fields[5]));
    item.Price = ToInt(fields[6]);
    item.Limit = (std::max)(0, ToInt(fields[7]));
    item.Bought = (std::max)(0, ToInt(fields[8]));
    item.Bank = fields[9] == L"1";
    m_incomingItems.push_back(std::move(item));
}

void Shop::TakeAnswer(const std::vector<std::wstring>& fields)
{
    m_buying = false;
    m_lastBuyOk = !fields.empty() && fields[0] == L"1";
    m_lastBuyText = Rest(fields, 1);
    ++m_buyAnswers;
    ++m_version;
}

void Shop::ExpireBuy()
{
    if (m_buying && std::chrono::steady_clock::now() - m_buySentAt > BuyTimeout)
    {
        m_buying = false;
        ++m_version;
    }
}

void Shop::Request()
{
    ExpireBuy();
    const auto now = std::chrono::steady_clock::now();
    if (m_lastRequest != std::chrono::steady_clock::time_point{} && now - m_lastRequest < RequestInterval)
        return;
    if (!CanSendCommand())
        return;

    m_lastRequest = now;
    SendCommand(std::wstring(ShopCommand));
}

void Shop::Buy(const std::wstring& key, int count)
{
    ExpireBuy();
    if (m_buying || key.empty() || count < 1 || count > MaximumBuyCount || !CanSendCommand())
        return;

    m_buying = true;
    m_buySentAt = std::chrono::steady_clock::now();
    m_lastBuyText.clear();
    ++m_version;
    SendCommand(std::wstring(BuyCommand) + key + L" " + std::to_wstring(count));
}

int Shop::MostBuyable(const ShopItem& item) const
{
    if (item.Price <= 0 || m_status.Balance <= 0)
        return 0;

    long long most = (std::min)(m_status.Balance / item.Price, static_cast<long long>(MaximumBuyCount));
    if (item.Limit > 0)
        most = (std::min)(most, static_cast<long long>((std::max)(0, item.Limit - item.Bought)));
    return static_cast<int>(most);
}

void Shop::Reset()
{
    m_status = {};
    m_items.clear();
    m_incomingStatus = {};
    m_incomingItems.clear();
    m_receiving = false;
    m_hasShop = false;
    m_buying = false;
    m_lastBuyOk = false;
    m_lastBuyText.clear();
    ++m_version;
    m_lastRequest = {};
}
} // namespace GameLogic::EndlessTower
