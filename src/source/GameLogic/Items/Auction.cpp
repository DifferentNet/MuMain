#include "stdafx.h"
#include "GameLogic/Items/Auction.h"

#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <algorithm>
#include <cwchar>
#include <string_view>

namespace GameLogic::Auction
{
namespace
{
constexpr std::wstring_view SettingsLine = L"#AUO|";
constexpr std::wstring_view PageHeader = L"#AUL0|";
constexpr std::wstring_view PageEnd = L"#AUL.|";
constexpr std::wstring_view LotLine = L"#AUL|";
constexpr std::wstring_view OwnHeader = L"#AUM0|";
constexpr std::wstring_view OwnEnd = L"#AUM.|";
constexpr std::wstring_view OwnLine = L"#AUM|";
constexpr std::wstring_view ResultLine = L"#AUR|";
constexpr std::wstring_view SoldLine = L"#AUN|";
constexpr std::wstring_view Command = L"/auc ";
// The settings' bit mask holds the kinds 0 to 30.
constexpr int MaskBits = 31;

std::vector<std::wstring> Split(std::wstring_view text)
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (start <= text.size())
    {
        const size_t bar = text.find(L'|', start);
        if (bar == std::wstring_view::npos)
        {
            fields.emplace_back(text.substr(start));
            break;
        }
        fields.emplace_back(text.substr(start, bar - start));
        start = bar + 1;
    }
    return fields;
}

long long ToNumber(const std::wstring& field)
{
    return std::wcstoll(field.c_str(), nullptr, 10);
}

int ToInt(const std::wstring& field)
{
    return static_cast<int>(ToNumber(field));
}

// "4:3,3:2" (kinds from 1) -> {kind 3, 3}, {kind 2, 2}; a malformed line is left out.
Price ParsePrice(const std::wstring& text)
{
    Price price;
    size_t start = 0;
    while (start < text.size())
    {
        const size_t comma = text.find(L',', start);
        const std::wstring part = text.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
        start = comma == std::wstring::npos ? text.size() : comma + 1;
        const size_t colon = part.find(L':');
        if (colon == std::wstring::npos)
            continue;
        const int kind = ToInt(part.substr(0, colon)) - 1;
        const int count = ToInt(part.substr(colon + 1));
        if (kind >= 0 && count > 0)
            price.push_back({kind, count});
    }
    return price;
}

std::vector<int> ParseList(const std::wstring& text)
{
    std::vector<int> values;
    size_t start = 0;
    while (start < text.size())
    {
        const size_t comma = text.find(L',', start);
        const int value = ToInt(text.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start));
        if (value > 0)
            values.push_back(value);
        start = comma == std::wstring::npos ? text.size() : comma + 1;
    }
    return values;
}

OwnLotState ParseState(const std::wstring& state)
{
    if (state == L"active")
        return OwnLotState::Active;
    if (state == L"sold")
        return OwnLotState::Sold;
    if (state == L"unsold")
        return OwnLotState::Unsold;
    if (state == L"leading")
        return OwnLotState::Leading;
    if (state == L"outbid")
        return OwnLotState::Outbid;
    if (state == L"won")
        return OwnLotState::Won;
    return OwnLotState::Busy;
}

// "3:7" (kind from 1, the leading bid) -> kind 2, count 7; empty or malformed: no bids (kind -1).
void ParseBid(const std::wstring& text, int& kind, int& count)
{
    kind = -1;
    count = 0;
    const size_t colon = text.find(L':');
    if (colon == std::wstring::npos)
        return;
    const int number = ToInt(text.substr(0, colon));
    if (number < 1)
        return;
    kind = number - 1;
    count = std::max(0, ToInt(text.substr(colon + 1)));
}

void SendCommand(const std::wstring& command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command.c_str()));
}
} // namespace

bool Settings::Allows(int kind) const
{
    return kind >= 0 && kind < MaskBits && (kinds >> kind & 1u) != 0;
}

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    // The longer prefixes first: "#AUL0|" and "#AUL.|" aren't lots.
    if (view.starts_with(SettingsLine))
        TakeSettings(Split(view.substr(SettingsLine.size())));
    else if (view.starts_with(PageHeader))
        TakePageHeader(Split(view.substr(PageHeader.size())));
    else if (view.starts_with(PageEnd))
        FinishPage();
    else if (view.starts_with(LotLine))
        TakeLot(Split(view.substr(LotLine.size())));
    else if (view.starts_with(OwnHeader))
        TakeOwnHeader(Split(view.substr(OwnHeader.size())));
    else if (view.starts_with(OwnEnd))
        FinishOwnLots();
    else if (view.starts_with(OwnLine))
        TakeOwnLot(Split(view.substr(OwnLine.size())));
    else if (view.starts_with(ResultLine))
        TakeResult(Split(view.substr(ResultLine.size())));
    else if (view.starts_with(SoldLine))
        TakeNotice(Split(view.substr(SoldLine.size())));
    else
        return false;
    return true;
}

void Store::TakeSettings(const std::vector<std::wstring>& fields)
{
    constexpr size_t SettingsFields = 8;
    if (fields.size() < SettingsFields)
        return;

    m_settings.feePerDay = ToNumber(fields[0]);
    m_settings.durations = ParseList(fields[1]);
    m_settings.maximumLots = ToInt(fields[2]);
    m_settings.onSale = ToInt(fields[3]);
    m_settings.priceLines = ToInt(fields[4]);
    m_settings.maximumCount = ToInt(fields[5]);
    m_settings.kinds = static_cast<unsigned>(ToNumber(fields[6]));
    m_settings.pageSize = ToInt(fields[7]);
    // The bids' settings came later: an older server sends none.
    constexpr size_t BidFields = 11;
    if (fields.size() >= BidFields)
    {
        m_settings.bidStepPercent = ToInt(fields[8]);
        m_settings.bidMinimumStep = ToInt(fields[9]);
        m_settings.bidExtendMinutes = ToInt(fields[10]);
    }
    m_hasSettings = true;
    // Raul's talk opens the window; the window's own question only updates it.
    if (!m_settingsAsked)
        m_openRequested = true;
    m_settingsAsked = false;
}

void Store::TakePageHeader(const std::vector<std::wstring>& fields)
{
    constexpr size_t HeaderFields = 3;
    m_foundIncoming = {};
    m_receivingFound = fields.size() >= HeaderFields;
    if (!m_receivingFound)
        return;
    m_foundIncoming.total = ToInt(fields[0]);
    m_foundIncoming.page = ToInt(fields[1]);
    m_foundIncoming.pages = ToInt(fields[2]);
}

void Store::TakeLot(const std::vector<std::wstring>& fields)
{
    constexpr size_t LotFields = 8;
    if (!m_receivingFound || fields.size() < LotFields)
        return;

    Lot lot;
    lot.number = ToInt(fields[0]);
    lot.version = ToInt(fields[1]);
    lot.item = StringUtils::HexToBytes(fields[2]);
    lot.price = ParsePrice(fields[3]);
    lot.secondsLeft = ToNumber(fields[4]);
    lot.seller = fields[5];
    lot.combatPower = ToNumber(fields[6]);
    lot.own = fields[7] == L"1";
    constexpr size_t BidFields = 12;
    if (fields.size() >= BidFields)
    {
        ParseBid(fields[8], lot.bidKind, lot.bidCount);
        lot.bids = ToInt(fields[9]);
        lot.nextBid = ToInt(fields[10]);
        lot.leading = fields[11] == L"1";
    }
    if (!lot.item.empty())
        m_foundIncoming.lots.push_back(std::move(lot));
}

void Store::FinishPage()
{
    if (!m_receivingFound)
        return;
    m_found = std::move(m_foundIncoming);
    m_found.receivedAt = std::chrono::steady_clock::now();
    m_foundIncoming = {};
    m_receivingFound = false;
    ++m_foundVersion;
}

void Store::TakeOwnHeader(const std::vector<std::wstring>& fields)
{
    constexpr size_t HeaderFields = 3;
    m_mineIncoming = {};
    m_receivingMine = fields.size() >= HeaderFields;
    if (!m_receivingMine)
        return;
    m_mineIncoming.count = ToInt(fields[0]);
    m_mineIncoming.onSale = ToInt(fields[1]);
    m_mineIncoming.maximumLots = ToInt(fields[2]);
}

void Store::TakeOwnLot(const std::vector<std::wstring>& fields)
{
    constexpr size_t LotFields = 6;
    if (!m_receivingMine || fields.size() < LotFields)
        return;

    OwnLot lot;
    lot.number = ToInt(fields[0]);
    lot.state = ParseState(fields[1]);
    lot.item = StringUtils::HexToBytes(fields[2]);
    lot.price = ParsePrice(fields[3]);
    lot.secondsLeft = ToNumber(fields[4]);
    lot.other = fields[5];
    constexpr size_t BidFields = 10;
    if (fields.size() >= BidFields)
    {
        lot.bidder = fields[6] == L"b";
        ParseBid(fields[7], lot.bidKind, lot.bidCount);
        lot.bids = ToInt(fields[8]);
        lot.mine = ToInt(fields[9]);
    }
    if (!lot.item.empty())
        m_mineIncoming.lots.push_back(std::move(lot));
}

void Store::FinishOwnLots()
{
    if (!m_receivingMine)
        return;
    m_mine = std::move(m_mineIncoming);
    m_mine.receivedAt = std::chrono::steady_clock::now();
    m_mineIncoming = {};
    m_receivingMine = false;
    ++m_mineVersion;
}

void Store::TakeResult(const std::vector<std::wstring>& fields)
{
    constexpr size_t ResultFields = 3;
    if (fields.size() < ResultFields)
        return;

    // "/auc refund" is sent without the window: its answers are chat lines only.
    if (fields[1] == L"refund")
        return;
    m_result.ok = fields[0] == L"ok";
    m_result.op = fields[1];
    // The text is the rest: it may hold a bar.
    std::wstring text = fields[2];
    for (size_t i = ResultFields; i < fields.size(); ++i)
        text += L"|" + fields[i];
    m_result.text = std::move(text);
    ++m_resultVersion;
}

void Store::TakeNotice(const std::vector<std::wstring>& fields)
{
    // A beaten bid's jewels wait in the auction: asked back at once, wherever the player is (the server answers
    // with chat lines and the jewel bank's counts).
    if (!fields.empty() && fields[0] == L"outbid")
        Send(L"refund");
    ++m_soldVersion;
}

bool Store::TakeOpenRequest()
{
    const bool requested = m_openRequested;
    m_openRequested = false;
    return requested;
}

void Store::Send(const std::wstring& request)
{
    SendCommand(std::wstring(Command) + request);
}

void Store::RequestSettings()
{
    m_settingsAsked = true;
    Send(L"info");
}

void Store::Reset()
{
    m_settings = {};
    m_hasSettings = false;
    m_openRequested = false;
    m_settingsAsked = false;
    m_found = {};
    m_foundIncoming = {};
    m_receivingFound = false;
    m_mine = {};
    m_mineIncoming = {};
    m_receivingMine = false;
    m_result = {};
    // The versions only grow, so a window never takes an old answer for a new one.
    ++m_foundVersion;
    ++m_mineVersion;
}
} // namespace GameLogic::Auction
